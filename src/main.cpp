// Crammy opens study windows side by side from profiles in profiles.txt.
//
//   crammy                                    the launcher
//   crammy --setup "ECE 150: Exam"            the launcher, on that setup
//   crammy --setup "ECE 150: Exam" --pick F24-F F24-M [--separate]
//   crammy --setup A --pick x --setup B --pick y   each setup on its own desktop, opened in one go
//   crammy --list                             every setup
//   crammy --list "ECE 150: Exam"             the PDFs that setup can pick
//   crammy --check                            problems in profiles.txt
//   crammy --edit                             the profile editor
//   crammy --background [--show]              the tray icon and shortcut, with no window
//   crammy --panel                            shows the panel of the background Crammy

#include <windows.h>
#include <objbase.h>

#include "background.h"
#include "command_line.h"
#include "launch.h"
#include "tiling.h"
#include "ui.h"

namespace {

HANDLE output() {
    HANDLE handle = GetStdHandle( STD_OUTPUT_HANDLE );
    if ( handle == nullptr || handle == INVALID_HANDLE_VALUE ) {
        if ( AttachConsole( ATTACH_PARENT_PROCESS ) ) {
            handle = CreateFileW( L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr );
            SetStdHandle( STD_OUTPUT_HANDLE, handle );
        }
    }
    return ( handle == INVALID_HANDLE_VALUE ) ? nullptr : handle;
}

// Prints to the console when there is one; shows a message box otherwise.
void say( const wstring &text ) {
    HANDLE handle = output();
    if ( !handle ) {
        MessageBoxW( nullptr, text.c_str(), L"Crammy", MB_OK );
        return;
    }
    wstring line = text + L"\r\n";
    DWORD mode, written;
    if ( GetConsoleMode( handle, &mode ) ) {
        WriteConsoleW( handle, line.c_str(), static_cast<DWORD>( line.size() ), &written, nullptr );
    } else {
        std::string bytes = to_utf8( line );
        WriteFile( handle, bytes.data(), static_cast<DWORD>( bytes.size() ), &written, nullptr );
    }
}

void say_all( const vector<wstring> &lines ) {
    wstring text;
    for ( const wstring &line : lines ) text += ( text.empty() ? L"" : L"\r\n" ) + line;
    if ( !text.empty() ) say( text );
}

bool has_pick( const setup &chosen ) {
    for ( const layout_item &item : chosen.items ) if ( needs_ticks( chosen, item ) ) return true;
    return false;
}

// True for the items --list shows and --pick can choose from: pick items, and sequence items
// even when a place is saved, so a deck can be named in place of the next one.
bool can_pick( const layout_item &item ) {
    return item.kind == item_kind::pick || item.kind == item_kind::sequence;
}

int list_pdfs( const setup &chosen ) {
    vector<wstring> lines;
    for ( const layout_item &item : chosen.items ) {
        if ( !can_pick( item ) ) continue;
        lines.push_back( L"[" + item.side + L"]" );
        for ( const pdf_file &pdf : pick_files( item ) ) {
            lines.push_back( date_text( pdf.modified ) + L"  " + pdf.folder + L"\\" + pdf.name );
        }
    }
    if ( lines.empty() ) lines.push_back( L"This setup has nothing to pick." );
    say_all( lines );
    return 0;
}

// A --pick text means the PDF with exactly that name, or else the newest one whose name contains it.
bool find_pick( const setup &chosen, const wstring &text, size_t &item_index, wstring &path ) {
    for ( bool exact : { true, false } ) {
        for ( size_t i = 0; i < chosen.items.size(); ++i ) {
            if ( !can_pick( chosen.items[i] ) ) continue;
            for ( const pdf_file &pdf : pick_files( chosen.items[i] ) ) {
                bool matches = exact ? lower( stem( pdf.name ) ) == lower( text ) : contains_ci( pdf.name, text );
                if ( matches ) {
                    item_index = i;
                    path = pdf.path;
                    return true;
                }
            }
        }
    }
    return false;
}

// Turns the --pick texts into picks for each item of the setup. False, with what is wrong, when one matches nothing.
bool resolve_picks( const setup &chosen, const vector<wstring> &pick_texts, vector<vector<wstring>> &picks ) {
    picks.assign( chosen.items.size(), {} );
    for ( const wstring &text : pick_texts ) {
        size_t item_index;
        wstring path;
        if ( !find_pick( chosen, text, item_index, path ) ) {
            say( L"No PDF to pick matches '" + text + L"' in " + chosen.title + L"." );
            return false;
        }
        picks[item_index].push_back( path );
    }
    return true;
}

// Started detached, so that the new copy does not end with whatever ran --panel, such as a taskbar button.
void start_background_with_panel() {
    wchar_t path[MAX_PATH * 4];
    DWORD length = GetModuleFileNameW( nullptr, path, MAX_PATH * 4 );
    wstring exe( path, length ), folder = parent_folder( exe );
    for ( DWORD flags : { DWORD( DETACHED_PROCESS ), DWORD( DETACHED_PROCESS | CREATE_BREAKAWAY_FROM_JOB ) } ) {
        wstring command = quoted( exe ) + L" --background --show";
        STARTUPINFOW startup{ sizeof( startup ) };
        PROCESS_INFORMATION info{};
        if ( CreateProcessW( exe.c_str(), command.data(), nullptr, nullptr, FALSE, flags, nullptr, folder.c_str(),
                             &startup, &info ) ) {
            CloseHandle( info.hThread );
            CloseHandle( info.hProcess );
            return;
        }
    }
}

}  // namespace

int WINAPI wWinMain( HINSTANCE, HINSTANCE, PWSTR, int ) {
    OleInitialize( nullptr );   // drag and drop needs OLE, not just COM

    int count = 0;
    wchar_t **arguments = CommandLineToArgvW( GetCommandLineW(), &count );
    vector<wstring> words;
    for ( int i = 1; i < count; ++i ) words.push_back( arguments[i] );
    LocalFree( arguments );

    command_line command = parse_command_line( words );
    if ( !command.error.empty() ) { say( command.error ); return 2; }
    const wstring &action = command.action;
    bool show = command.show;

    if ( action == L"background" ) {
        if ( background_is_running() ) {
            if ( show ) ask_running_crammy_to_show_panel();
            return 0;
        }
        return run_background( show );
    }

    if ( action == L"panel" ) {
        if ( !ask_running_crammy_to_toggle_panel() ) start_background_with_panel();
        return 0;
    }

    if ( action == L"edit" ) {
        show_editor( nullptr );
        run_message_loop();
        return 0;
    }

    profile_file profiles = load_profiles();

    if ( action == L"check" ) {
        if ( profiles.errors.empty() ) say( std::to_wstring( profiles.setups.size() ) + L" setups, no problems." );
        else say_all( profiles.errors );
        return profiles.errors.empty() ? 0 : 1;
    }

    if ( action == L"list" ) {
        if ( !command.list_title.empty() ) {
            const setup *chosen = find_setup( profiles, command.list_title );
            if ( !chosen ) { say( L"No single setup matches '" + command.list_title + L"'. Try --list." ); return 1; }
            return list_pdfs( *chosen );
        }
        vector<wstring> titles;
        for ( const setup &each : profiles.setups ) titles.push_back( each.title );
        say_all( titles );
        return 0;
    }

    vector<setup_to_open> to_open;
    for ( const setup_request &request : command.setups ) {
        const setup *chosen = find_setup( profiles, request.title );
        if ( !chosen ) { say( L"No single setup matches '" + request.title + L"'. Try --list." ); return 1; }
        setup_to_open each{ chosen, {}, request.separate };
        if ( !resolve_picks( *chosen, request.picks, each.picks ) ) return 1;
        to_open.push_back( each );
    }

    // One setup that still needs ticks opens the launcher on it; otherwise everything opens now
    const setup *chosen = to_open.empty() ? nullptr : to_open[0].chosen;
    bool needs_launcher = to_open.size() == 1 && command.setups[0].picks.empty() && has_pick( *chosen );
    if ( !to_open.empty() && !needs_launcher ) {
        vector<wstring> problems = open_setups( to_open );
        say_all( problems );
        return problems.empty() ? 0 : 1;
    }

    show_main_window( chosen ? chosen->title : L"" );
    run_message_loop();
    return 0;
}
