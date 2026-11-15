#include "next_runner.h"

#include <algorithm>
#include <map>

#include "background.h"
#include "desktops.h"
#include "dialogs.h"
#include "files.h"
#include "launch.h"
#include "next_line.h"
#include "panel.h"
#include "profiles.h"
#include "sequence.h"
#include "slide_watch.h"
#include "subjects.h"
#include "timetable.h"

namespace {

const UINT_PTR line_timer = 101;
const UINT_PTR watch_timer = 102;
const UINT_PTR desktop_timer = 103;
const UINT line_every_ms = 10 * 1000;   // often enough that "in Nm" is never a minute behind
const UINT watch_every_ms = 2 * 1000;
const UINT desktop_every_ms = 500;      // the taskbar box follows a desktop switch within half a second

HWND background = nullptr;
bool busy = false;      // a Next or next deck is opening windows, or the tick list is open

// The last line worked out, kept so a desktop switch can set the title without working it out again
wstring last_line;
bool last_next_can_act = false;
int shown_desktop_index = -1;   // the desktop whose name is in the title, or -1

// Every window Next opened in this run, with the file it shows
vector<opened_window> records;

// A newest line Next opened, watched for a newer file
struct slide_watch {
    watched_slide slide;
    opened_window shown;    // the window showing slide.open_path
    wstring setup_title;
    wstring desktop;        // replacing happens only while this desktop shows
    calendar_date started;
    std::map<wstring, std::pair<unsigned long long, ULONGLONG>> sizes;  // path: size, and when it last changed
};
vector<slide_watch> watches;

struct moment {
    calendar_date date;
    int minute;
};

moment now() {
    SYSTEMTIME time;
    GetLocalTime( &time );
    return { { time.wYear, time.wMonth, time.wDay }, time.wHour * 60 + time.wMinute };
}

next_class current_next( const moment &at ) {
    vector<wstring> errors;
    vector<class_block> blocks;
    if ( is_file( timetable_path() ) ) blocks = read_timetable( read_text_file( timetable_path() ), errors );
    return find_next_class( blocks, at.date, at.minute, known_courses( blocks ) );
}

// The lecture decks opened today for the next class's sequence setup, read from decks today.txt,
// which every way of opening a setup writes
struct today_decks {
    bool on = false;            // decks were opened today for the next class
    wstring setup_title;
    layout_item item;           // the sequence line
    wstring desktop;            // the class's desktop
    vector<pdf_file> found;     // the decks in the folder, in section order
    vector<wstring> opened;     // paths of today's decks still in the folder, in the order opened
    wstring highest;            // the name of the highest of them, or the place when none is left
};

today_decks decks_are_on( const next_class &next ) {
    today_decks decks;
    if ( !next.found || !next.today ) return decks;

    profile_file profiles = load_profiles();
    vector<wstring> titles;
    for ( const setup &each : profiles.setups ) titles.push_back( each.title );
    const setup *chosen = find_setup( profiles, setup_for_class( next.meeting.course, next.meeting.kind, titles ) );
    if ( !chosen ) return decks;
    bool has_sequence = false;
    for ( const layout_item &item : chosen->items ) {
        if ( item.kind != item_kind::sequence ) continue;
        decks.item = item;
        has_sequence = true;
    }
    if ( !has_sequence ) return decks;

    decks_on_day record = saved_decks_today( chosen->title );
    if ( record.date != today_text() || record.files.empty() ) return decks;

    decks.on = true;
    decks.setup_title = chosen->title;
    decks.desktop = desktop_name_for( next.meeting.course );
    decks.found = pick_files( decks.item );
    decks.highest = saved_place( chosen->title );
    bool any_left = false;
    for ( const wstring &file : record.files ) {
        for ( const pdf_file &pdf : decks.found ) {
            if ( lower( pdf.name ) != lower( file ) ) continue;
            decks.opened.push_back( pdf.path );
            if ( !any_left || deck_before( decks.highest, pdf.name ) ) decks.highest = pdf.name;
            any_left = true;
        }
    }
    return decks;
}

// Presses of Next that arrived while busy were queued; they are dropped, not run afterwards
void drop_queued_presses() {
    MSG message;
    while ( background && PeekMessageW( &message, background, next_message(), next_message(), PM_REMOVE ) ) {}
}

void stop_being_busy() {
    drop_queued_presses();
    busy = false;
}

vector<wstring> names_of( const vector<pdf_file> &pdfs ) {
    vector<wstring> names;
    for ( const pdf_file &pdf : pdfs ) names.push_back( pdf.name );
    return names;
}

bool same_path( const wstring &a, const wstring &b ) {
    return lower( a ) == lower( b );
}

bool is_one_of( const wstring &path, const vector<wstring> &paths ) {
    for ( const wstring &each : paths )
        if ( same_path( each, path ) ) return true;
    return false;
}

// Closes a window Next opened, unless it has gone, belongs to another program now, or is the one to keep
void close_if_ours( const opened_window &window, HWND keep ) {
    if ( window.window != keep && still_ours( window.window, window.process_id ) )
        PostMessageW( window.window, WM_CLOSE, 0, 0 );
}

wstring control_folder() {
    return join_path( expand_variables( L"%TEMP%" ), L"crammy" );
}

// A file for PdfCraft's control channel, so its window can be driven with pdfcraft-cli ui
wstring new_control_file() {
    static int count = 0;
    return join_path( control_folder(), L"pdfcraft control " + std::to_wstring( GetCurrentProcessId() ) + L"-" +
                                            std::to_wstring( ++count ) + L".json" );
}

void delete_old_control_files() {
    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW( join_path( control_folder(), L"pdfcraft control *.json" ).c_str(), &data );
    if ( handle == INVALID_HANDLE_VALUE ) return;
    do {
        DeleteFileW( join_path( control_folder(), data.cFileName ).c_str() );
    } while ( FindNextFileW( handle, &data ) );
    FindClose( handle );
}

unsigned long long file_time_number( const FILETIME &time ) {
    return ( static_cast<unsigned long long>( time.dwHighDateTime ) << 32 ) | time.dwLowDateTime;
}

unsigned long long modified_time( const wstring &path ) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if ( !GetFileAttributesExW( path.c_str(), GetFileExInfoStandard, &data ) ) return 0;
    return file_time_number( data.ftLastWriteTime );
}

vector<recorded_window> recorded_now() {
    vector<recorded_window> windows;
    for ( const opened_window &record : records )
        windows.push_back( { record.path, still_ours( record.window, record.process_id ) } );
    return windows;
}

void start_watches( const setup &chosen, const vector<opened_window> &opened, const wstring &desktop, calendar_date today ) {
    for ( const layout_item &item : chosen.items ) {
        if ( item.kind != item_kind::newest ) continue;
        for ( const opened_window &window : opened ) {
            if ( !matches_pattern( window.path, item.paths[0] ) ) continue;
            watched_slide slide{ item.paths[0], window.path, modified_time( window.path ) };
            watches.push_back( { slide, window, chosen.title, desktop, today, {} } );
            break;
        }
    }
}

// The PDFs in the watched folder, with how long since each one's size last changed
vector<seen_file> files_seen( slide_watch &watch ) {
    vector<seen_file> files;
    ULONGLONG tick = GetTickCount64();
    wstring folder = parent_folder( watch.slide.pattern );
    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW( join_path( folder, L"*.pdf" ).c_str(), &data );
    if ( handle == INVALID_HANDLE_VALUE ) return files;
    do {
        if ( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) continue;
        wstring path = join_path( folder, data.cFileName );
        unsigned long long size = ( static_cast<unsigned long long>( data.nFileSizeHigh ) << 32 ) | data.nFileSizeLow;
        auto &last = watch.sizes[lower( path )];
        if ( last.second == 0 || last.first != size ) last = { size, tick };
        files.push_back( { path, file_time_number( data.ftLastWriteTime ), size, tick - last.second } );
    } while ( FindNextFileW( handle, &data ) );
    FindClose( handle );
    return files;
}

// Opens a newer file over the old window, then closes the old window. Nothing takes the focus.
// Returns false when the new window never appeared, so the watch stops.
bool replace_if_newer( slide_watch &watch ) {
    wstring path = replacement_for( watch.slide, files_seen( watch ) );
    if ( path.empty() ) return true;

    busy = true;
    vector<wstring> problems;
    planned_window window{ watch.shown.side, watch.shown.app, { path } };
    vector<opened_window> opened = open_window_in_place_of( window, watch.shown.window, problems );
    stop_being_busy();
    if ( opened.empty() ) return false;

    close_if_ours( watch.shown, opened[0].window );
    records.push_back( opened[0] );
    watch.slide.open_path = path;
    watch.slide.open_modified = modified_time( path );
    watch.shown = opened[0];
    return true;
}

void check_watches() {
    if ( next_is_busy() || watches.empty() ) return;
    moment at = now();
    wstring desktop = current_desktop_name();
    for ( size_t i = 0; i < watches.size(); ) {
        slide_watch &watch = watches[i];
        bool ended = watch_has_ended( watch.started, at.date ) || !still_ours( watch.shown.window, watch.shown.process_id );
        if ( !ended && same_path( desktop, watch.desktop ) ) ended = !replace_if_newer( watch );
        if ( ended ) watches.erase( watches.begin() + i );
        else ++i;
    }
}

// Sets the title from the last line and the given desktop, only when it differs from the one there.
// Returns false when the desktop's name could not be read, which happens for a moment while Windows switches
// or makes a desktop; the name shown before is kept then, so the box does not blink off.
bool show_title( int desktop_index ) {
    if ( !background ) return false;
    static wstring last_desktop;
    bool found = false;
    if ( desktop_index >= 0 ) {
        vector<wstring> names = desktop_names();
        if ( desktop_index < int( names.size() ) ) {
            last_desktop = names[desktop_index];
            found = true;
        }
    }
    const wstring &desktop = last_desktop;
    wstring title = taskbar_title( desktop, last_line, last_next_can_act );

    wstring current( GetWindowTextLengthW( background ) + 1, L' ' );
    current.resize( GetWindowTextW( background, &current[0], int( current.size() ) ) );
    if ( current != title ) SetWindowTextW( background, title.c_str() );
    return found;
}

// Reads which desktop shows, which is a registry read, and sets the title again only when it changed
void check_desktop() {
    int index = current_desktop_index();
    if ( index == shown_desktop_index ) return;
    // Remember the index only once its name was read, so a later tick tries again
    if ( show_title( index ) ) shown_desktop_index = index;
}

void add_problem( wstring &problem, const wstring &more ) {
    if ( !more.empty() ) problem += ( problem.empty() ? L"" : L" " ) + more;
}

// What Next does; returns the problems to show, or ""
wstring open_next() {
    moment at = now();
    next_class next = current_next( at );
    if ( !next.found || !next.known ) return L"";

    profile_file profiles = load_profiles();
    vector<wstring> titles;
    for ( const setup &each : profiles.setups ) titles.push_back( each.title );
    // No tiles are passed, so the plan names the setup even when the desktop has windows on it
    class_click click = plan_class_click( next.meeting, desktop_names(), {}, titles );
    wstring problem;
    if ( !go_to_class_desktop( click, problem ) ) return problem;
    wstring desktop = current_desktop_name();

    const setup *found = click.setup_title.empty() ? nullptr : find_setup( profiles, click.setup_title );
    if ( !found ) {
        watches.clear();
        return problem;
    }
    setup chosen = *found;
    chosen.desktop.clear();     // already there

    pick_answer picks;
    bool asked = false;
    for ( const layout_item &item : chosen.items ) asked = asked || needs_ticks( chosen, item );
    if ( asked ) {
        picks = ask_for_picks( nullptr, chosen );
        if ( !picks.accepted ) return problem;
        // He may have moved to another desktop while the tick list was open
        wstring switch_problem = switch_to_desktop( desktop );
        if ( !switch_problem.empty() ) {
            add_problem( problem, switch_problem );
            return problem;
        }
    }
    vector<wstring> problems;
    vector<planned_window> plan = plan_setup( chosen, picks.picks, picks.separate, problems );

    const layout_item *sequence = nullptr;
    for ( const layout_item &item : chosen.items )
        if ( item.kind == item_kind::sequence ) sequence = &item;
    vector<wstring> deck_paths;
    if ( sequence )
        for ( const pdf_file &pdf : pick_files( *sequence ) ) deck_paths.push_back( pdf.path );

    // plan_setup already plans the decks opened today, from decks today.txt, so a second press
    // does not move the place on. A window of decks gets a control file.
    for ( planned_window &window : plan ) {
        bool all_decks = window.app == program::pdfcraft && !window.paths.empty();
        for ( const wstring &path : window.paths ) all_decks = all_decks && is_one_of( path, deck_paths );
        if ( all_decks ) window.control_file = new_control_file();
    }

    vector<wstring> files;
    for ( const planned_window &window : plan ) files.insert( files.end(), window.paths.begin(), window.paths.end() );
    if ( all_already_open( files, recorded_now() ) ) {
        // Only switch: everything is showing. Watches of other setups end here.
        watches.erase( std::remove_if( watches.begin(), watches.end(),
                                       [&]( const slide_watch &watch ) { return watch.setup_title != chosen.title; } ),
                       watches.end() );
        return problem;
    }

    vector<opened_window> opened = plan.empty() ? vector<opened_window>() : open_plan( chosen, plan, problems );
    records.insert( records.end(), opened.begin(), opened.end() );
    watches.clear();
    start_watches( chosen, opened, desktop, at.date );
    add_problem( problem, save_places( chosen, opened ) );     // as open_setup does

    for ( const wstring &each : problems ) add_problem( problem, each );
    return problem;
}

}  // namespace

void start_next_runner( HWND background_window ) {
    background = background_window;
    delete_old_control_files();
    refresh_line();
    SetTimer( background, line_timer, line_every_ms, nullptr );
    SetTimer( background, watch_timer, watch_every_ms, nullptr );
    SetTimer( background, desktop_timer, desktop_every_ms, nullptr );
}

void on_next_timer( UINT_PTR timer_id ) {
    if ( timer_id == line_timer ) refresh_line();
    else if ( timer_id == watch_timer ) check_watches();
    else if ( timer_id == desktop_timer ) check_desktop();
}

void run_as_busy( const std::function<void()> &work ) {
    busy = true;
    work();
    stop_being_busy();
}

bool next_is_busy() {
    // A tick list open anywhere in Crammy, such as one from the panel, also counts
    return busy || FindWindowW( L"CrammyPick", nullptr ) != nullptr;
}

void remember_windows( const vector<opened_window> &opened ) {
    records.insert( records.end(), opened.begin(), opened.end() );
}

void refresh_line() {
    if ( !background ) return;
    moment at = now();
    next_class next = current_next( at );
    today_decks decks = decks_are_on( next );
    wstring deck = decks.on ? deck_label( decks.highest ) : L"";
    last_line = line_text( next, at.minute, deck );
    last_next_can_act = next.found && next.known;
    // The names are read again here, so a renamed desktop shows within 10 seconds
    int index = current_desktop_index();
    shown_desktop_index = show_title( index ) ? index : -1;
}

void run_next() {
    if ( next_is_busy() ) return;
    busy = true;
    wstring problem = open_next();
    stop_being_busy();
    refresh_line();
    if ( !problem.empty() ) show_panel_problem( problem );
}

wstring next_deck_label() {
    today_decks decks = decks_are_on( current_next( now() ) );
    if ( !decks.on ) return L"";
    return deck_after( names_of( decks.found ), decks.highest ) == decks.highest ? L"last deck" : L"next deck";
}

void run_next_deck() {
    if ( next_is_busy() ) return;
    today_decks decks = decks_are_on( current_next( now() ) );
    if ( !decks.on ) return;
    wstring next_name = deck_after( names_of( decks.found ), decks.highest );
    wstring next_path;
    for ( const pdf_file &pdf : decks.found )
        if ( pdf.name == next_name ) next_path = pdf.path;
    if ( next_name == decks.highest || next_path.empty() ) return;     // the last deck: nothing to add

    busy = true;
    wstring problem = switch_to_desktop( decks.desktop );
    if ( problem.empty() ) {
        // pdfcraft-cli has no method known to add a tab to a running window without trying it on
        // screen, so a new window opens with every deck of today as a tab, and the old one closes.
        vector<wstring> paths = decks.opened;
        paths.push_back( next_path );
        vector<wstring> problems;
        planned_window window{ decks.item.side, program::pdfcraft, paths, new_control_file() };
        vector<opened_window> opened = open_windows_recorded( { window }, problems );
        if ( opened.empty() ) {
            problem = problems.empty() ? L"No window appeared for " + next_name : problems[0];
        } else {
            // The old deck windows are the ones this run opened that show one of today's decks
            for ( const opened_window &old : records ) {
                if ( old.app == program::pdfcraft && is_one_of( old.path, decks.opened ) )
                    close_if_ours( old, opened[0].window );
            }
            records.insert( records.end(), opened.begin(), opened.end() );
            if ( !save_decks_today( decks.setup_title, { next_name } ) ) problem = decks_today_problem;
            else if ( !save_place( decks.setup_title, next_name ) ) problem = place_problem;
        }
    }
    stop_being_busy();
    refresh_line();
    if ( !problem.empty() ) show_panel_problem( problem );
}
