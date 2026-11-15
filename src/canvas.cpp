#include "canvas.h"

#include <ole2.h>
#include <shellapi.h>
#include <windowsx.h>

#include <map>

#include "dialogs.h"
#include "files.h"
#include "tiling.h"
#include "ui.h"

namespace {

enum zone { zone_none = -1, zone_left, zone_right, zone_full };
const wchar_t *zone_sides[] = { L"left", L"right", L"full" };

// A tile is drawn as a small copy of the window it stands for
struct tile {
    size_t item_index;
    RECT box;
    RECT remove_box;        // the × in its title bar
    program app;
    wstring opens;          // the file or folder it will show
    wstring caption;        // a short note under that, or nothing
};

struct canvas_state {
    HWND window = nullptr;
    HFONT font = nullptr, bold_font = nullptr;
    UINT dpi = 96;
    vector<written_item> *items = nullptr;
    wstring root;
    vector<tile> tiles;
    zone hover = zone_none;     // the zone something is being dragged over
    int pressed_tile = -1;      // the tile under the held mouse button
    bool dragging = false;
    POINT press_point{};
    int split = 50;             // the left side's share of the width in percent
    side_asker asker = nullptr; // nullptr means the menu and dialogs below
    bool tracking_leave = false;
};

canvas_state canvas;

int px( int value ) {
    return scaled( value, canvas.dpi );
}

void hint( const wchar_t *text ) {
    SendMessageW( GetParent( canvas.window ), canvas_hint_message, 0, reinterpret_cast<LPARAM>( text ) );
}

bool has_side( const wchar_t *side ) {
    if ( !canvas.items ) return false;
    for ( const written_item &item : *canvas.items ) if ( item.side == side ) return true;
    return false;
}

wstring full_path( const wstring &path ) {
    wstring expanded = expand_variables( path );
    return is_absolute( expanded ) ? expanded : join_path( canvas.root, expanded );
}

// The screen keeps the shape of the real one and fits inside the canvas
RECT screen_area() {
    RECT client, work;
    GetClientRect( canvas.window, &client );
    SystemParametersInfoW( SPI_GETWORKAREA, 0, &work, 0 );
    double ratio = double( work.right - work.left ) / double( work.bottom - work.top );
    int width = client.right - 2 * px( 12 ), height = client.bottom - 2 * px( 12 );
    if ( width / ratio > height ) width = int( height * ratio );
    else height = int( width / ratio );
    int left = ( client.right - width ) / 2, top = ( client.bottom - height ) / 2;
    return { left, top, left + width, top + height };
}

RECT zone_box( zone place ) {
    RECT screen = screen_area();
    int middle = screen.left + ( screen.right - screen.left ) * canvas.split / 100;
    if ( place == zone_left ) return { screen.left, screen.top, middle, screen.bottom };
    if ( place == zone_right ) return { middle, screen.top, screen.right, screen.bottom };
    return screen;
}

// On a split screen the two halves are the targets. Otherwise the middle third means the whole screen.
zone zone_at( POINT point ) {
    RECT screen = screen_area();
    double x = double( point.x - screen.left ) / double( screen.right - screen.left );
    if ( has_side( L"left" ) || has_side( L"right" ) ) return x * 100 < canvas.split ? zone_left : zone_right;
    if ( x < 1.0 / 3.0 ) return zone_left;
    if ( x > 2.0 / 3.0 ) return zone_right;
    return zone_full;
}

// Putting a window on one side moves any whole-screen windows to the other side
wstring side_for( zone place ) {
    if ( place == zone_full ) return L"full";
    const wchar_t *other = place == zone_left ? L"right" : L"left";
    for ( written_item &item : *canvas.items ) if ( item.side == L"full" ) item.side = other;
    return zone_sides[place];
}

wstring folder_label( const wstring &path ) {
    return path == L"." ? file_name( canvas.root ) : file_name( path );
}

void describe( tile &each, const written_item &item ) {
    each.app = item.app == viewer::acrobat ? program::acrobat : program::pdfcraft;
    size_t count = item.paths.size();
    switch ( item.kind ) {
    case item_kind::notes:
        each.app = program::notepad_pp;
        each.opens = count ? file_name( item.paths[0] ) : L"Answer notes";
        if ( !count ) each.caption = L"Named after the first PDF";
        break;
    case item_kind::folder:
        each.app = program::explorer;
        each.opens = folder_label( item.paths[0] );
        break;
    case item_kind::pdf:
        each.opens = file_name( item.paths[0] );
        break;
    case item_kind::newest: {
        vector<pdf_file> matches = find_pdfs( { full_path( item.paths[0] ) }, true );
        each.opens = matches.empty() ? L"No file yet" : matches[0].name;
        each.caption = L"Opens the newest one each time";
        break;
    }
    case item_kind::tabs:
        each.opens = file_name( item.paths[0] );
        if ( count > 1 ) each.caption = L"and " + std::to_wstring( count - 1 ) + L" more, as tabs";
        break;
    case item_kind::pick:
        each.opens = L"You choose when opening";
        each.caption = L"From ";
        for ( size_t i = 0; i < count; ++i ) each.caption += ( i ? L", " : L"" ) + folder_label( item.paths[i] );
        break;
    case item_kind::sequence:
        each.opens = L"The next numbered deck";
        each.caption = L"In order, from " + folder_label( item.paths[0] );
        break;
    case item_kind::web:
        each.app = program::chrome;
        each.opens = web_label( item.paths[0] );
        if ( count > 1 ) each.caption = L"and " + std::to_wstring( count - 1 ) + L" more, as tabs";
        break;
    }
}

void lay_out_tiles() {
    canvas.tiles.clear();
    if ( !canvas.items ) return;
    int gap = px( 6 );
    for ( zone place : { zone_left, zone_right, zone_full } ) {
        vector<size_t> on_side;
        for ( size_t i = 0; i < canvas.items->size(); ++i ) {
            if ( ( *canvas.items )[i].side == zone_sides[place] ) on_side.push_back( i );
        }
        if ( on_side.empty() ) continue;
        RECT area = zone_box( place );
        int row_height = ( area.bottom - area.top ) / int( on_side.size() );
        for ( size_t row = 0; row < on_side.size(); ++row ) {
            int top = area.top + int( row ) * row_height;
            RECT box = { area.left + gap, top + gap, area.right - gap, top + row_height - gap };
            RECT remove_box = { box.right - px( 32 ), box.top, box.right, box.top + px( 28 ) };
            tile each{ on_side[row], box, remove_box };
            describe( each, ( *canvas.items )[ on_side[row] ] );
            canvas.tiles.push_back( each );
        }
    }
}

void changed() {
    lay_out_tiles();
    InvalidateRect( canvas.window, nullptr, FALSE );
    SendMessageW( GetParent( canvas.window ), canvas_changed_message, 0, 0 );
}

void set_hover( zone place ) {
    if ( place == canvas.hover ) return;
    canvas.hover = place;
    InvalidateRect( canvas.window, nullptr, FALSE );
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

const wchar_t *program_names[] = { L"PdfCraft", L"Adobe Acrobat", L"Notepad++", L"File Explorer", L"Google Chrome" };

HICON program_icon( program app, int size ) {
    static std::map<std::pair<int, int>, HICON> icons;
    HICON &icon = icons[{ int( app ), size }];
    if ( !icon ) PrivateExtractIconsW( program_file( app ), 0, size, size, &icon, nullptr, 1, 0 );
    return icon;
}

COLORREF mix( COLORREF a, COLORREF b, double amount_of_b ) {
    auto channel = [&]( int x, int y ) { return int( x + ( y - x ) * amount_of_b ); };
    return RGB( channel( GetRValue( a ), GetRValue( b ) ), channel( GetGValue( a ), GetGValue( b ) ),
                channel( GetBValue( a ), GetBValue( b ) ) );
}

void round_box( HDC dc, const RECT &box, COLORREF fill, COLORREF edge, int edge_width, int radius ) {
    HBRUSH brush = CreateSolidBrush( fill );
    HPEN pen = CreatePen( PS_SOLID, edge_width, edge );
    HGDIOBJ old_brush = SelectObject( dc, brush ), old_pen = SelectObject( dc, pen );
    RoundRect( dc, box.left, box.top, box.right, box.bottom, radius, radius );
    SelectObject( dc, old_brush );
    SelectObject( dc, old_pen );
    DeleteObject( brush );
    DeleteObject( pen );
}

void write( HDC dc, const wstring &text, RECT box, HFONT font, COLORREF colour, UINT format ) {
    SelectObject( dc, font );
    SetTextColor( dc, colour );
    DrawTextW( dc, text.c_str(), -1, &box, format | DT_NOPREFIX );
}

void paint( HDC dc ) {
    RECT client;
    GetClientRect( canvas.window, &client );
    FillRect( dc, &client, GetSysColorBrush( COLOR_BTNFACE ) );
    SetBkMode( dc, TRANSPARENT );

    // The monitor: a dark frame around a plain desktop
    RECT screen = screen_area();
    RECT frame = screen;
    InflateRect( &frame, px( 6 ), px( 6 ) );
    round_box( dc, frame, RGB( 36, 36, 36 ), RGB( 36, 36, 36 ), 1, px( 8 ) );
    round_box( dc, screen, RGB( 228, 231, 236 ), RGB( 228, 231, 236 ), 1, 0 );

    COLORREF accent = GetSysColor( COLOR_HIGHLIGHT );
    if ( canvas.hover != zone_none ) {
        RECT target = zone_box( canvas.hover );
        InflateRect( &target, -px( 4 ), -px( 4 ) );
        round_box( dc, target, mix( RGB( 255, 255, 255 ), accent, 0.2 ), accent, px( 2 ), px( 6 ) );
    }

    const wchar_t *empty_text = !canvas.items ? L"Choose a setup on the left"
                              : canvas.items->empty() ? L"Click to choose, or drop PDFs, folders or links here"
                              : nullptr;
    if ( empty_text ) write( dc, empty_text, screen, canvas.font, RGB( 110, 110, 110 ), DT_CENTER | DT_VCENTER | DT_SINGLELINE );
    if ( !canvas.items ) return;

    // A side with nothing on it says it can be clicked
    for ( zone place : { zone_left, zone_right } ) {
        bool empty = !canvas.items->empty() && !has_side( L"full" ) && !has_side( zone_sides[place] );
        if ( empty ) write( dc, L"Click to add", zone_box( place ), canvas.font, RGB( 110, 110, 110 ), DT_CENTER | DT_VCENTER | DT_SINGLELINE );
    }

    for ( size_t i = 0; i < canvas.tiles.size(); ++i ) {
        const tile &each = canvas.tiles[i];
        bool lifted = canvas.dragging && int( i ) == canvas.pressed_tile;
        round_box( dc, each.box, lifted ? RGB( 240, 240, 240 ) : RGB( 255, 255, 255 ), RGB( 190, 190, 190 ), 1, px( 6 ) );

        // Title bar with the program's icon and name, and a close button
        RECT bar = { each.box.left + 1, each.box.top + 1, each.box.right - 1, each.box.top + px( 28 ) };
        round_box( dc, bar, RGB( 243, 243, 243 ), RGB( 243, 243, 243 ), 1, px( 5 ) );
        int small = px( 16 );
        DrawIconEx( dc, bar.left + px( 8 ), bar.top + ( bar.bottom - bar.top - small ) / 2, program_icon( each.app, small ),
                    small, small, 0, nullptr, DI_NORMAL );
        RECT bar_text = { bar.left + px( 32 ), bar.top, each.remove_box.left, bar.bottom };
        write( dc, program_names[ int( each.app ) ], bar_text, canvas.font, RGB( 60, 60, 60 ),
               DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS );
        write( dc, L"\x2715", each.remove_box, canvas.font, RGB( 60, 60, 60 ), DT_CENTER | DT_VCENTER | DT_SINGLELINE );

        // Body: the big icon, then what it opens, then the note
        RECT body = { each.box.left + px( 10 ), bar.bottom, each.box.right - px( 10 ), each.box.bottom - px( 6 ) };
        int big = ( body.bottom - body.top ) > px( 150 ) ? px( 48 ) : px( 32 );
        int text_height = px( 44 );
        int top = body.top + ( body.bottom - body.top - big - text_height ) / 2;
        DrawIconEx( dc, ( body.left + body.right - big ) / 2, top, program_icon( each.app, big ), big, big, 0, nullptr, DI_NORMAL );
        RECT name_box = { body.left, top + big + px( 8 ), body.right, top + big + px( 28 ) };
        write( dc, each.opens, name_box, canvas.bold_font, RGB( 20, 20, 20 ), DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS );
        RECT caption_box = { body.left, name_box.bottom, body.right, name_box.bottom + px( 20 ) };
        write( dc, each.caption, caption_box, canvas.font, RGB( 110, 110, 110 ), DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS );
    }
}

// ---------------------------------------------------------------------------
// Changing items
// ---------------------------------------------------------------------------

// paths are files and folders; addresses are web pages, from a link dragged out of a browser
void add_dropped( const vector<wstring> &paths, zone place, vector<wstring> addresses = {} ) {
    vector<wstring> pdfs, folders, notes;
    for ( const wstring &path : paths ) {
        wstring stored = relative_to( canvas.root, path );
        if ( is_folder( path ) ) folders.push_back( stored );
        else if ( ends_with( lower( path ), L".pdf" ) ) pdfs.push_back( stored );
        else if ( ends_with( lower( path ), L".txt" ) ) notes.push_back( stored );
        else if ( ends_with( lower( path ), L".url" ) ) {
            wstring address = web_address( address_in_shortcut( read_text_file( path ) ) );
            if ( !address.empty() ) addresses.push_back( address );
        }
    }
    if ( pdfs.empty() && folders.empty() && notes.empty() && addresses.empty() ) {
        hint( L"Only PDFs, folders, .txt notes and web links can go on the screen." );
        return;
    }

    wstring side = side_for( place );
    for ( const wstring &folder : folders ) canvas.items->push_back( { side, item_kind::pick, { folder } } );
    if ( pdfs.size() == 1 ) canvas.items->push_back( { side, item_kind::pdf, pdfs } );
    if ( pdfs.size() > 1 ) canvas.items->push_back( { side, item_kind::tabs, pdfs } );
    for ( const wstring &note : notes ) canvas.items->push_back( { side, item_kind::notes, { note } } );
    if ( !addresses.empty() ) canvas.items->push_back( { side, item_kind::web, addresses } );
    changed();

    if ( !folders.empty() ) hint( L"A folder lets you tick which PDFs to open each time. Right-click it for more." );
    else if ( pdfs.empty() && notes.empty() ) hint( L"Web pages open in a new Chrome window." );
    else hint( L"Right-click a tile to open it in Acrobat, or to always open the newest file like it." );
}

// ---------------------------------------------------------------------------
// Clicking an empty part of the screen
// ---------------------------------------------------------------------------

enum side_menu_id { side_pdf = 1, side_newest, side_sequence, side_pick, side_folder, side_web, side_notes };

// The usual asker: a menu of what can go there, then the dialog that choice needs
side_answer ask_with_menu( HWND window, const wstring &, const wstring &start_folder ) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW( menu, MF_STRING, side_pdf, L"PDF\x2026" );
    AppendMenuW( menu, MF_STRING, side_newest, L"Newest PDF in a folder\x2026" );
    AppendMenuW( menu, MF_STRING, side_sequence, L"Next lecture deck in a folder\x2026" );
    AppendMenuW( menu, MF_STRING, side_pick, L"Tick PDFs from a folder each time\x2026" );
    AppendMenuW( menu, MF_SEPARATOR, 0, nullptr );
    AppendMenuW( menu, MF_STRING, side_folder, L"Folder in File Explorer\x2026" );
    AppendMenuW( menu, MF_STRING, side_web, L"Web page in Chrome\x2026" );
    AppendMenuW( menu, MF_STRING, side_notes, L"Answer notes" );
    POINT cursor;
    GetCursorPos( &cursor );
    int choice = TrackPopupMenu( menu, TPM_RETURNCMD, cursor.x, cursor.y, 0, window, nullptr );
    DestroyMenu( menu );

    side_answer answer;
    HWND owner = GetAncestor( window, GA_ROOT );
    switch ( choice ) {
    case side_pdf:
        answer.choice = side_choice::pdf;
        answer.paths = ask_for_pdfs( owner, start_folder );
        break;
    case side_newest:
    case side_sequence:
    case side_pick:
    case side_folder: {
        const wchar_t *titles[] = { L"Folder to open the newest PDF from", L"Folder of numbered lecture decks",
                                    L"Folder to tick PDFs from", L"Folder to open in File Explorer" };
        const side_choice choices[] = { side_choice::newest, side_choice::sequence, side_choice::pick, side_choice::folder };
        int index = choice - side_newest;
        answer.choice = choices[index];
        wstring folder = ask_for_folder( owner, start_folder, titles[index] );
        if ( !folder.empty() ) answer.paths = { folder };
        break;
    }
    case side_web: {
        answer.choice = side_choice::web;
        wstring typed = ask_for_address( owner );
        if ( !typed.empty() ) answer.paths = { typed };
        break;
    }
    case side_notes:
        answer.choice = side_choice::notes;
        answer.accepted = true;
        return answer;
    default:
        return answer;
    }
    answer.accepted = !answer.paths.empty();
    return answer;
}

// The side a point on the canvas would fill, or "" when it is not over an empty part of the screen
wstring empty_side_under( POINT point ) {
    if ( !canvas.items ) return L"";
    RECT screen = screen_area();
    if ( !PtInRect( &screen, point ) ) return L"";
    double across = double( point.x - screen.left ) / double( screen.right - screen.left );
    return empty_side_at( *canvas.items, across, canvas.split );
}

zone zone_named( const wstring &side ) {
    return side == L"left" ? zone_left : side == L"right" ? zone_right : side == L"full" ? zone_full : zone_none;
}

void fill_empty_side( const wstring &side ) {
    side_asker asker = canvas.asker ? canvas.asker : ask_with_menu;
    side_answer answer = asker( canvas.window, side, start_folder_for( canvas.root ) );
    if ( !answer.accepted || !canvas.items ) return;
    written_item item;
    wstring problem;
    if ( !item_for_answer( answer, side, canvas.root, item, problem ) ) {
        hint( problem.c_str() );
        return;
    }
    canvas.items->push_back( item );
    changed();
    hint( L"Drag a tile to move it, or right-click it for more." );
}

void move_item( size_t index, zone place ) {
    written_item item = ( *canvas.items )[index];
    canvas.items->erase( canvas.items->begin() + index );
    item.side = side_for( place );
    canvas.items->push_back( item );
    changed();
}

enum menu_id { menu_pdfcraft = 1, menu_acrobat, menu_newest, menu_pick, menu_explorer, menu_left, menu_right, menu_full, menu_remove };

void show_menu( size_t index, POINT screen_point ) {
    written_item &item = ( *canvas.items )[index];
    bool shows_pdfs = item.kind != item_kind::notes && item.kind != item_kind::folder;
    HMENU menu = CreatePopupMenu();

    if ( shows_pdfs ) {
        AppendMenuW( menu, MF_STRING | ( item.app == viewer::pdfcraft ? MF_CHECKED : 0 ), menu_pdfcraft, L"Open with PdfCraft" );
        AppendMenuW( menu, MF_STRING | ( item.app == viewer::acrobat ? MF_CHECKED : 0 ), menu_acrobat, L"Open with Adobe Acrobat" );
        AppendMenuW( menu, MF_SEPARATOR, 0, nullptr );
    }
    if ( item.kind == item_kind::pdf || item.kind == item_kind::newest ) {
        bool possible = item.kind == item_kind::newest || !newest_pattern( item.paths[0] ).empty();
        AppendMenuW( menu, MF_STRING | ( item.kind == item_kind::newest ? MF_CHECKED : 0 ) | ( possible ? 0 : MF_GRAYED ),
                     menu_newest, L"Always open the newest file like this" );
    }
    if ( item.kind == item_kind::folder ) AppendMenuW( menu, MF_STRING, menu_pick, L"Let me tick PDFs from it when opening" );
    if ( item.kind == item_kind::pick && item.paths.size() == 1 && is_folder( full_path( item.paths[0] ) ) ) {
        AppendMenuW( menu, MF_STRING, menu_explorer, L"Open the folder in File Explorer" );
    }
    AppendMenuW( menu, MF_SEPARATOR, 0, nullptr );
    AppendMenuW( menu, MF_STRING, menu_left, L"Move to the left half" );
    AppendMenuW( menu, MF_STRING, menu_right, L"Move to the right half" );
    AppendMenuW( menu, MF_STRING, menu_full, L"Make it the whole screen" );
    AppendMenuW( menu, MF_SEPARATOR, 0, nullptr );
    AppendMenuW( menu, MF_STRING, menu_remove, L"Remove" );

    int choice = TrackPopupMenu( menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen_point.x, screen_point.y, 0, canvas.window, nullptr );
    DestroyMenu( menu );

    switch ( choice ) {
    case menu_pdfcraft: item.app = viewer::pdfcraft; break;
    case menu_acrobat: item.app = viewer::acrobat; break;
    case menu_newest:
        if ( item.kind == item_kind::pdf ) {
            item.kind = item_kind::newest;
            item.paths = { newest_pattern( item.paths[0] ) };
        } else {
            vector<pdf_file> matches = find_pdfs( { full_path( item.paths[0] ) }, true );
            if ( matches.empty() ) return;
            item.kind = item_kind::pdf;
            item.paths = { relative_to( canvas.root, matches[0].path ) };
        }
        break;
    case menu_pick: item.kind = item_kind::pick; break;
    case menu_explorer: item.kind = item_kind::folder; break;
    case menu_left: move_item( index, zone_left ); return;
    case menu_right: move_item( index, zone_right ); return;
    case menu_full:
        // The whole screen only makes sense when nothing else is on a side
        for ( written_item &other : *canvas.items ) other.side = L"full";
        break;
    case menu_remove: canvas.items->erase( canvas.items->begin() + index ); break;
    default: return;
    }
    changed();
}

// ---------------------------------------------------------------------------
// Files dragged in from File Explorer
// ---------------------------------------------------------------------------

POINT client_point( POINTL screen_point ) {
    POINT point = { screen_point.x, screen_point.y };
    ScreenToClient( canvas.window, &point );
    return point;
}

FORMATETC file_format() {
    return { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
}

// What a browser hands over when a link or the address is dragged out of it
FORMATETC link_format() {
    static CLIPFORMAT format = static_cast<CLIPFORMAT>( RegisterClipboardFormatW( L"UniformResourceLocatorW" ) );
    return { format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
}

bool has_format( IDataObject *data, FORMATETC format ) {
    return data->QueryGetData( &format ) == S_OK;
}

wstring dropped_link( IDataObject *data ) {
    FORMATETC format = link_format();
    STGMEDIUM medium{};
    if ( FAILED( data->GetData( &format, &medium ) ) ) return L"";
    const wchar_t *text = static_cast<const wchar_t *>( GlobalLock( medium.hGlobal ) );
    wstring link = text ? wstring( text, wcsnlen( text, GlobalSize( medium.hGlobal ) / sizeof( wchar_t ) ) ) : L"";
    GlobalUnlock( medium.hGlobal );
    ReleaseStgMedium( &medium );
    return web_address( link );
}

vector<wstring> dropped_paths( IDataObject *data ) {
    vector<wstring> paths;
    FORMATETC format = file_format();
    STGMEDIUM medium{};
    if ( FAILED( data->GetData( &format, &medium ) ) ) return paths;
    HDROP drop = static_cast<HDROP>( GlobalLock( medium.hGlobal ) );
    UINT count = DragQueryFileW( drop, 0xFFFFFFFF, nullptr, 0 );
    for ( UINT i = 0; i < count; ++i ) {
        wchar_t path[MAX_PATH * 4];
        if ( DragQueryFileW( drop, i, path, MAX_PATH * 4 ) ) paths.push_back( path );
    }
    GlobalUnlock( medium.hGlobal );
    ReleaseStgMedium( &medium );
    return paths;
}

class drop_target : public IDropTarget {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface( REFIID id, void **out ) override {
        if ( id == IID_IUnknown || id == IID_IDropTarget ) {
            *out = this;
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    // One drop target exists for the whole run, so counting references is not needed
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }

    HRESULT STDMETHODCALLTYPE DragEnter( IDataObject *data, DWORD keys, POINTL point, DWORD *effect ) override {
        accepts_ = canvas.items && ( has_format( data, file_format() ) || has_format( data, link_format() ) );
        if ( !canvas.items ) hint( L"Pick a setup on the left first, or press New setup." );
        else if ( accepts_ ) hint( L"Let go over the part of the screen where it should open." );
        return DragOver( keys, point, effect );
    }
    HRESULT STDMETHODCALLTYPE DragOver( DWORD, POINTL point, DWORD *effect ) override {
        *effect = accepts_ ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        set_hover( accepts_ ? zone_at( client_point( point ) ) : zone_none );
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override {
        set_hover( zone_none );
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop( IDataObject *data, DWORD, POINTL point, DWORD *effect ) override {
        zone place = zone_at( client_point( point ) );
        set_hover( zone_none );
        *effect = accepts_ ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        if ( !accepts_ ) return S_OK;
        if ( has_format( data, file_format() ) ) {
            add_dropped( dropped_paths( data ), place );
        } else {
            wstring address = dropped_link( data );
            if ( address.empty() ) hint( L"That link is not a web address." );
            else add_dropped( {}, place, { address } );
        }
        return S_OK;
    }

private:
    bool accepts_ = false;
};

drop_target dropper;

// ---------------------------------------------------------------------------
// Mouse and window messages
// ---------------------------------------------------------------------------

int tile_at( POINT point ) {
    for ( int i = int( canvas.tiles.size() ) - 1; i >= 0; --i ) {
        if ( PtInRect( &canvas.tiles[i].box, point ) ) return i;
    }
    return -1;
}

LRESULT CALLBACK canvas_proc( HWND window, UINT message, WPARAM wparam, LPARAM lparam ) {
    POINT point = { GET_X_LPARAM( lparam ), GET_Y_LPARAM( lparam ) };
    switch ( message ) {
    case WM_PAINT: {
        PAINTSTRUCT paint_info;
        HDC screen_dc = BeginPaint( window, &paint_info );
        RECT client;
        GetClientRect( window, &client );
        HDC dc = CreateCompatibleDC( screen_dc );
        HBITMAP bitmap = CreateCompatibleBitmap( screen_dc, client.right, client.bottom );
        HGDIOBJ old_bitmap = SelectObject( dc, bitmap );
        paint( dc );
        BitBlt( screen_dc, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY );
        SelectObject( dc, old_bitmap );
        DeleteObject( bitmap );
        DeleteDC( dc );
        EndPaint( window, &paint_info );
        return 0;
    }
    case WM_PRINTCLIENT:
        paint( reinterpret_cast<HDC>( wparam ) );
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        lay_out_tiles();
        InvalidateRect( window, nullptr, FALSE );
        return 0;
    case WM_LBUTTONDOWN: {
        int index = tile_at( point );
        if ( index >= 0 && PtInRect( &canvas.tiles[index].remove_box, point ) ) {
            canvas.items->erase( canvas.items->begin() + canvas.tiles[index].item_index );
            changed();
            return 0;
        }
        canvas.pressed_tile = index;
        canvas.press_point = point;
        if ( index >= 0 ) SetCapture( window );
        return 0;
    }
    case WM_MOUSEMOVE:
        if ( canvas.pressed_tile >= 0 && !canvas.dragging &&
             ( std::abs( point.x - canvas.press_point.x ) > px( 6 ) || std::abs( point.y - canvas.press_point.y ) > px( 6 ) ) ) {
            canvas.dragging = true;
        }
        if ( canvas.dragging ) {
            set_hover( zone_at( point ) );
        } else if ( canvas.pressed_tile < 0 ) {
            // An empty part of the screen lights up to show a click will fill it
            set_hover( zone_named( empty_side_under( point ) ) );
            if ( !canvas.tracking_leave ) {
                TRACKMOUSEEVENT track{ sizeof( track ), TME_LEAVE, window, 0 };
                canvas.tracking_leave = TrackMouseEvent( &track );
            }
        }
        return 0;
    case WM_MOUSELEAVE:
        canvas.tracking_leave = false;
        if ( !canvas.dragging ) set_hover( zone_none );
        return 0;
    case WM_SETCURSOR:
        if ( LOWORD( lparam ) == HTCLIENT && !canvas.dragging ) {
            POINT cursor;
            GetCursorPos( &cursor );
            ScreenToClient( window, &cursor );
            if ( !empty_side_under( cursor ).empty() ) {
                SetCursor( LoadCursorW( nullptr, IDC_HAND ) );
                return TRUE;
            }
        }
        break;
    case WM_LBUTTONUP: {
        bool was_dragging = canvas.dragging;
        int index = canvas.pressed_tile;
        zone place = canvas.hover;
        canvas.dragging = false;
        canvas.pressed_tile = -1;
        set_hover( zone_none );
        ReleaseCapture();
        if ( was_dragging && index >= 0 && place != zone_none ) move_item( canvas.tiles[index].item_index, place );
        else if ( index >= 0 ) hint( L"Drag a tile to move it, or right-click it for more." );
        else if ( !was_dragging ) {
            wstring side = empty_side_under( point );
            if ( !side.empty() ) fill_empty_side( side );
        }
        return 0;
    }
    case WM_CAPTURECHANGED:
        if ( canvas.dragging ) {
            canvas.dragging = false;
            canvas.pressed_tile = -1;
            set_hover( zone_none );
        }
        return 0;
    case WM_RBUTTONUP: {
        int index = tile_at( point );
        if ( index < 0 ) return 0;
        POINT screen_point = point;
        ClientToScreen( window, &screen_point );
        show_menu( canvas.tiles[index].item_index, screen_point );
        return 0;
    }
    case WM_DESTROY:
        RevokeDragDrop( window );
        DeleteObject( canvas.bold_font );
        return 0;
    }
    return DefWindowProcW( window, message, wparam, lparam );
}

}  // namespace

HWND create_canvas( HWND parent, HFONT font, UINT dpi ) {
    WNDCLASSEXW window_class{ sizeof( window_class ) };
    window_class.lpfnWndProc = canvas_proc;
    window_class.hInstance = GetModuleHandleW( nullptr );
    window_class.hCursor = LoadCursorW( nullptr, IDC_ARROW );
    window_class.lpszClassName = L"CrammyCanvas";
    RegisterClassExW( &window_class );

    canvas.dpi = dpi;
    canvas.font = font;
    LOGFONTW bold;
    GetObjectW( font, sizeof( bold ), &bold );
    bold.lfWeight = FW_SEMIBOLD;
    canvas.bold_font = CreateFontIndirectW( &bold );

    canvas.window = CreateWindowExW( 0, window_class.lpszClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, parent,
                                     nullptr, window_class.hInstance, nullptr );
    RegisterDragDrop( canvas.window, &dropper );
    return canvas.window;
}

void show_items( vector<written_item> *items, const wstring &root ) {
    canvas.items = items;
    canvas.root = root;
    canvas.pressed_tile = -1;
    canvas.dragging = false;
    lay_out_tiles();
    InvalidateRect( canvas.window, nullptr, FALSE );
}

IDropTarget *canvas_drop_target() {
    return &dropper;
}

void set_canvas_split( int split ) {
    canvas.split = split >= 10 && split <= 90 ? split : 50;
    lay_out_tiles();
    if ( canvas.window ) InvalidateRect( canvas.window, nullptr, FALSE );
}

void set_side_asker( side_asker asker ) {
    canvas.asker = asker;
}

void add_notes_tile() {
    if ( !canvas.items ) return;
    wstring side = has_side( L"left" ) && !has_side( L"right" ) ? L"right" : side_for( zone_left );
    canvas.items->push_back( { side, item_kind::notes, {} } );
    changed();
    hint( L"Notes open in Notepad++, in a file named after the first PDF of the setup." );
}
