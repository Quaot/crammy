#include "launch.h"

#include "desktops.h"
#include "notes_tabs.h"
#include "sequence.h"
#include "tiling.h"

vector<pdf_file> pick_files( const layout_item &item ) {
    if ( item.kind != item_kind::sequence ) return find_pdfs( item.paths, true );

    vector<pdf_file> found = find_pdfs( item.paths, false );
    vector<wstring> names;
    for ( const pdf_file &pdf : found ) names.push_back( pdf.name );

    vector<pdf_file> decks;
    for ( const wstring &name : deck_order( names ) ) {
        for ( const pdf_file &pdf : found ) {
            if ( pdf.name == name ) { decks.push_back( pdf ); break; }
        }
    }
    return decks;
}

// The place saved for this setup, or "" when there is none
static wstring saved_place( const setup &chosen ) {
    if ( !is_file( places_path() ) ) return L"";
    std::map<wstring, wstring> places = read_places( read_text_file( places_path() ) );
    auto found = places.find( chosen.title );
    return found == places.end() ? L"" : found->second;
}

bool needs_ticks( const setup &chosen, const layout_item &item ) {
    if ( item.kind == item_kind::pick ) return true;
    return item.kind == item_kind::sequence && saved_place( chosen ).empty();
}

vector<planned_window> plan_setup( const setup &chosen, const vector<vector<wstring>> &picks, bool separate,
                                   vector<wstring> &problems ) {
    vector<planned_window> plan;
    wstring first_pdf;
    vector<std::pair<size_t, const layout_item *>> notes_windows;

    for ( size_t i = 0; i < chosen.items.size(); ++i ) {
        const layout_item &item = chosen.items[i];
        vector<wstring> pdfs;

        switch ( item.kind ) {
        case item_kind::notes:
            notes_windows.push_back( { plan.size(), &item } );
            plan.push_back( { item.side, program::notepad_pp, {} } );
            continue;
        case item_kind::folder:
            if ( is_folder( item.paths[0] ) ) plan.push_back( { item.side, program::explorer, item.paths } );
            else problems.push_back( L"Folder not found: " + item.paths[0] );
            continue;
        case item_kind::pdf:
            if ( is_file( item.paths[0] ) ) pdfs = item.paths;
            else problems.push_back( L"PDF not found: " + item.paths[0] );
            break;
        case item_kind::tabs:
            for ( const pdf_file &pdf : find_pdfs( item.paths, false ) ) pdfs.push_back( pdf.path );
            if ( pdfs.empty() ) problems.push_back( L"No PDFs found for the tabs on the " + item.side );
            break;
        case item_kind::pick:
            if ( i < picks.size() ) pdfs = picks[i];
            break;
        case item_kind::sequence: {
            // Ticked decks come first, as for pick: there is no place yet, or --pick named them
            if ( i < picks.size() && !picks[i].empty() ) {
                pdfs = picks[i];
                break;
            }
            wstring place = saved_place( chosen );
            if ( place.empty() ) break;
            vector<pdf_file> decks = pick_files( item );
            vector<wstring> names;
            for ( const pdf_file &deck : decks ) names.push_back( deck.name );
            wstring next = deck_after( names, place );
            for ( const pdf_file &deck : decks ) if ( deck.name == next ) pdfs = { deck.path };
            if ( pdfs.empty() ) problems.push_back( L"No numbered lecture decks found in " + item.paths[0] );
            break;
        }
        case item_kind::newest: {
            vector<pdf_file> matches = find_pdfs( item.paths, true );
            if ( matches.empty() ) {
                problems.push_back( L"No PDF fits " + file_name( item.paths[0] ) );
                break;
            }
            pdfs = { matches[0].path };
            if ( date_text( matches[0].modified ) != today_text() ) {
                problems.push_back( L"The newest " + file_name( item.paths[0] ) + L" is " + matches[0].name +
                                    L", from " + date_text( matches[0].modified ) + L". Today's may not be downloaded yet." );
            }
            break;
        }
        }
        if ( pdfs.empty() ) continue;
        if ( first_pdf.empty() ) first_pdf = pdfs[0];

        // Acrobat gets a window per PDF. PdfCraft shows several as tabs unless asked to separate them.
        bool one_window_each = item.app == viewer::acrobat || ( ( item.kind == item_kind::pick || item.kind == item_kind::sequence ) && separate );
        program app = item.app == viewer::acrobat ? program::acrobat : program::pdfcraft;
        if ( one_window_each ) {
            for ( const wstring &pdf : pdfs ) plan.push_back( { item.side, app, { pdf } } );
        } else {
            plan.push_back( { item.side, app, pdfs } );
        }
    }

    if ( plan.size() == notes_windows.size() ) {
        problems.push_back( L"Nothing to open. Tick at least one PDF." );
        return {};
    }

    // A notes file is named after the first PDF, which is only known now
    for ( const auto &[plan_index, notes_item] : notes_windows ) {
        plan[plan_index].paths = { notes_file_name( chosen, *notes_item, first_pdf ) };
    }
    return plan;
}

vector<opened_window> open_plan( const setup &chosen, const vector<planned_window> &plan, vector<wstring> &problems ) {
    vector<wstring> notes_paths;
    for ( const planned_window &window : plan ) {
        if ( window.app != program::notepad_pp ) continue;
        create_notes_file( window.paths[0] );
        notes_paths.push_back( window.paths[0] );
    }

    // Save and close the notes tabs from earlier setups, so Notepad++ does not fill up with them
    close_old_notes( notes_paths );
    for ( const wstring &path : notes_paths ) remember_notes( path );

    if ( !chosen.desktop.empty() ) {
        wstring problem = switch_to_desktop( chosen.desktop );
        if ( !problem.empty() ) problems.push_back( problem );
    }
    return open_windows_recorded( plan, problems );
}

static bool same_path( const wstring &a, const wstring &b ) {
    return lower( a ) == lower( b );
}

vector<wstring> decks_shown( const layout_item &item, const vector<opened_window> &opened ) {
    vector<wstring> shown;
    for ( const pdf_file &deck : pick_files( item ) ) {
        for ( const opened_window &window : opened ) {
            if ( window.app == program::pdfcraft && same_path( window.path, deck.path ) ) {
                shown.push_back( deck.path );
                break;
            }
        }
    }
    return shown;   // pick_files gives the decks in section order already
}

static unsigned long long file_size( const wstring &path ) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if ( !GetFileAttributesExW( path.c_str(), GetFileExInfoStandard, &data ) ) return 0;
    return ( static_cast<unsigned long long>( data.nFileSizeHigh ) << 32 ) | data.nFileSizeLow;
}

// Writes a new copy and then moves it over places.txt, so a failed write never leaves half a file.
// A places.txt that exists but cannot be read is left alone, since rewriting it would lose its places.
bool save_place( const wstring &title, const wstring &deck_name ) {
    wstring path = places_path();
    std::map<wstring, wstring> places;
    if ( is_file( path ) ) {
        wstring text = read_text_file( path );
        if ( text.empty() && file_size( path ) > 0 ) return false;
        places = read_places( text );
    }
    places[title] = deck_name;
    wstring temporary = path + L".tmp";
    return write_text_file( temporary, write_places( places ) ) &&
           MoveFileExW( temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING );
}

const wchar_t *place_problem = L"Could not save the place in places.txt.";

wstring save_places( const setup &chosen, const vector<opened_window> &opened ) {
    for ( const layout_item &item : chosen.items ) {
        if ( item.kind != item_kind::sequence ) continue;
        vector<wstring> shown = decks_shown( item, opened );
        if ( !shown.empty() && !save_place( chosen.title, file_name( shown.back() ) ) ) return place_problem;
    }
    return L"";
}

vector<wstring> open_setup( const setup &chosen, const vector<vector<wstring>> &picks, bool separate ) {
    vector<wstring> problems;
    vector<planned_window> plan = plan_setup( chosen, picks, separate, problems );
    if ( plan.empty() ) return problems;
    vector<opened_window> opened = open_plan( chosen, plan, problems );
    wstring problem = save_places( chosen, opened );
    if ( !problem.empty() ) problems.push_back( problem );
    return problems;
}
