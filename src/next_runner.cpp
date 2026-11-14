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
const UINT line_every_ms = 10 * 1000;   // often enough that "in Nm" is never a minute behind
const UINT watch_every_ms = 2 * 1000;

HWND background = nullptr;
bool busy = false;      // a Next or next deck is opening windows, or the tick list is open

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

// The lecture decks of a sequence setup that Next opened today
struct deck_run {
    wstring setup_title;            // "" until Next opens decks
    wstring course, kind;
    calendar_date date{};
    layout_item item;               // the sequence line
    vector<wstring> opened;         // deck paths, in section order
    vector<opened_window> windows;  // one entry for each window showing them
    wstring side;
    wstring desktop;
};
deck_run decks;

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

// True when the next class is the one today's decks were opened for
bool decks_are_on( const next_class &next, calendar_date today ) {
    return !decks.setup_title.empty() && !decks.opened.empty() && decks.date == today && next.found && next.today &&
           same_course( next.meeting.course, decks.course ) && lower( next.meeting.kind ) == lower( decks.kind );
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

unsigned long long file_size( const wstring &path ) {
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

// Remembers the decks a sequence line opened, and saves the highest as the place
wstring remember_decks( const setup &chosen, const layout_item &item, const next_class &next,
                        const vector<opened_window> &opened, const wstring &desktop, calendar_date today ) {
    vector<wstring> deck_paths;
    for ( const pdf_file &pdf : pick_files( item ) ) deck_paths.push_back( pdf.path );

    deck_run run{ chosen.title, next.meeting.course, next.meeting.kind, today, item, {}, {}, L"", desktop };
    for ( const opened_window &window : opened ) {
        if ( window.app != program::pdfcraft || !is_one_of( window.path, deck_paths ) ) continue;
        if ( !is_one_of( window.path, run.opened ) ) run.opened.push_back( window.path );
        bool listed = false;
        for ( const opened_window &each : run.windows ) listed = listed || each.window == window.window;
        if ( !listed ) run.windows.push_back( window );
        run.side = window.side;
    }
    if ( run.opened.empty() ) return L"";
    std::sort( run.opened.begin(), run.opened.end(),
               []( const wstring &a, const wstring &b ) { return deck_before( file_name( a ), file_name( b ) ); } );
    decks = run;
    return save_place( chosen.title, file_name( decks.opened.back() ) ) ? L"" : place_problem;
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

    // When decks were opened for this class today, they are the decks to show, so a second press
    // does not move the place on.
    bool same_decks = sequence && decks_are_on( next, at.date ) && decks.setup_title == chosen.title;
    bool decks_placed = false;
    for ( size_t i = 0; i < plan.size(); ) {
        planned_window &window = plan[i];
        bool all_decks = window.app == program::pdfcraft && !window.paths.empty();
        for ( const wstring &path : window.paths ) all_decks = all_decks && is_one_of( path, deck_paths );
        if ( all_decks && same_decks ) {
            if ( decks_placed ) {
                plan.erase( plan.begin() + i );
                continue;
            }
            window.paths = decks.opened;
            decks_placed = true;
        }
        if ( all_decks ) window.control_file = new_control_file();
        ++i;
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
    if ( sequence ) add_problem( problem, remember_decks( chosen, *sequence, next, opened, desktop, at.date ) );

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
}

void on_next_timer( UINT_PTR timer_id ) {
    if ( timer_id == line_timer ) refresh_line();
    else if ( timer_id == watch_timer ) check_watches();
}

bool next_is_busy() {
    // A tick list open anywhere in Crammy, such as one from the panel, also counts
    return busy || FindWindowW( L"CrammyPick", nullptr ) != nullptr;
}

void refresh_line() {
    if ( !background ) return;
    moment at = now();
    next_class next = current_next( at );
    wstring deck = decks_are_on( next, at.date ) ? deck_label( file_name( decks.opened.back() ) ) : L"";
    wstring title = background_title( line_text( next, at.minute, deck ), next.found && next.known );
    SetWindowTextW( background, title.c_str() );
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
    moment at = now();
    if ( !decks_are_on( current_next( at ), at.date ) ) return L"";
    wstring place = file_name( decks.opened.back() );
    return deck_after( names_of( pick_files( decks.item ) ), place ) == place ? L"last deck" : L"next deck";
}

void run_next_deck() {
    if ( next_is_busy() ) return;
    moment at = now();
    if ( !decks_are_on( current_next( at ), at.date ) ) return;
    vector<pdf_file> found = pick_files( decks.item );
    wstring place = file_name( decks.opened.back() );
    wstring next_name = deck_after( names_of( found ), place );
    wstring next_path;
    for ( const pdf_file &pdf : found )
        if ( pdf.name == next_name ) next_path = pdf.path;
    if ( next_name == place || next_path.empty() ) return;     // the last deck: nothing to add

    busy = true;
    wstring problem = switch_to_desktop( decks.desktop );
    if ( problem.empty() ) {
        // pdfcraft-cli has no method known to add a tab to a running window without trying it on
        // screen, so a new window opens with every deck of today as a tab, and the old one closes.
        vector<wstring> paths = decks.opened;
        paths.push_back( next_path );
        vector<wstring> problems;
        planned_window window{ decks.side, program::pdfcraft, paths, new_control_file() };
        vector<opened_window> opened = open_windows_recorded( { window }, problems );
        if ( opened.empty() ) {
            problem = problems.empty() ? L"No window appeared for " + next_name : problems[0];
        } else {
            for ( const opened_window &old : decks.windows ) close_if_ours( old, opened[0].window );
            records.insert( records.end(), opened.begin(), opened.end() );
            decks.windows = { opened[0] };
            decks.opened = paths;
            if ( !save_place( decks.setup_title, next_name ) ) problem = place_problem;
        }
    }
    stop_being_busy();
    refresh_line();
    if ( !problem.empty() ) show_panel_problem( problem );
}
