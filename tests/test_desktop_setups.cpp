// Written from tests\specs\desktop_setups.md.

#include <windows.h>

#include <cwctype>

#include "check.h"
#include "../src/command_line.h"
#include "../src/desktops.h"
#include "../src/notes_tabs.h"
#include "../src/profiles.h"
#include "../src/tiling.h"

namespace {

const wchar_t *window_class_name = L"CrammyDesktopSetupsTestWindow";

void register_window_class() {
    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = DefWindowProcW;
    window_class.hInstance = GetModuleHandleW( nullptr );
    window_class.lpszClassName = window_class_name;
    RegisterClassW( &window_class );
}

// An ordinary application window, made off screen and left hidden, so Explorer has not put it on a desktop.
HWND make_unshown_window( const wstring &title ) {
    return CreateWindowExW( WS_EX_APPWINDOW, window_class_name, title.c_str(), WS_OVERLAPPEDWINDOW,
                            -32000, -32000, 200, 100, nullptr, nullptr, GetModuleHandleW( nullptr ), nullptr );
}

// An ordinary application window, made off screen so it never appears on the owner's screen.
HWND make_window( const wstring &title ) {
    HWND window = make_unshown_window( title );
    if ( window )
        ShowWindow( window, SW_SHOWNOACTIVATE );
    return window;
}

wstring unique_title( const wstring &start ) {
    return start + L" " + std::to_wstring( GetCurrentProcessId() ) + L" " + std::to_wstring( GetTickCount64() );
}

// The desktop windows_by_desktop() reports the window on, or -1 when it is not listed.
int desktop_of( HWND window ) {
    for ( const window_on_desktop &entry : windows_by_desktop() )
        if ( entry.window == window )
            return entry.desktop_index;
    return -1;
}

// The same name with its letters in a different case, when it has letters.
wstring other_case( const wstring &name ) {
    wstring upper_name = name, lower_name = name;
    for ( wchar_t &letter : upper_name ) letter = (wchar_t)std::towupper( letter );
    for ( wchar_t &letter : lower_name ) letter = (wchar_t)std::towlower( letter );
    return upper_name != name ? upper_name : lower_name;
}

void check_move_to_same_desktop( HWND window, const vector<wstring> &names, int showing ) {
    wstring problem = move_window_to_desktop( window, names[showing] );
    check( problem.empty(), "Moving a window to the desktop it is already on returns no problem." );
    check( desktop_of( window ) == showing, "A window moved to the desktop it is already on stays on that desktop." );
    check( current_desktop_index() == showing,
           "Moving a window to the desktop it is already on leaves the same desktop showing." );
}

void check_move_to_unknown_desktop( HWND window, int showing ) {
    wstring unknown_name = unique_title( L"Crammy no such desktop" );
    wstring problem = move_window_to_desktop( window, unknown_name );
    check( !problem.empty(), "Moving a window to a desktop name that matches no desktop returns a problem." );
    check( problem.find( L"'" + unknown_name + L"'" ) != wstring::npos,
           "The problem for an unknown desktop name holds that name in single quotes." );
    check( desktop_of( window ) == showing, "A window moved to an unknown desktop name stays where it was." );
    check( current_desktop_index() == showing, "Moving a window to an unknown desktop name leaves the same desktop showing." );
}

void check_move_to_other_desktop( HWND window, const vector<wstring> &names, int showing ) {
    if ( names.size() < 2 ) {
        check( true, "There is only one desktop, so the checks that move a window to another desktop are skipped." );
        return;
    }
    int other = showing == 0 ? 1 : 0;
    wstring problem = move_window_to_desktop( window, names[other] );
    check( problem.empty(), "Moving the test window to another desktop returns no problem." );
    check( desktop_of( window ) == other, "After the move, windows_by_desktop() reports the test window on the other desktop." );
    check( current_desktop_index() == showing, "Moving the test window to another desktop leaves the same desktop showing." );

    wstring back_problem = move_window_to_desktop( window, other_case( names[showing] ) );
    check( back_problem.empty(), "Moving the test window back with the desktop name in another case returns no problem." );
    check( desktop_of( window ) == showing, "The test window is back on the desktop showing after the move back." );
    check( current_desktop_index() == showing, "Moving the test window back leaves the same desktop showing." );

    if ( desktop_of( window ) != showing )
        move_window_to_desktop( window, names[showing] );
}

void check_not_a_window( int showing ) {
    wstring null_problem = move_window_to_desktop( nullptr, desktop_names()[showing] );
    check( !null_problem.empty(), "Moving a null window handle returns a problem." );
    check( current_desktop_index() == showing, "Moving a null window handle leaves the same desktop showing." );

    HWND destroyed = make_window( unique_title( L"Crammy destroyed desktop setups check" ) );
    DestroyWindow( destroyed );
    wstring destroyed_problem = move_window_to_desktop( destroyed, desktop_names()[showing] );
    check( !destroyed_problem.empty(), "Moving a window that was already destroyed returns a problem." );
    check( current_desktop_index() == showing, "Moving a destroyed window leaves the same desktop showing." );
}

void check_window_never_shown( const vector<wstring> &names, int showing ) {
    HWND window = make_unshown_window( unique_title( L"Crammy unshown desktop setups check" ) );
    check( window != nullptr, "The hidden off-screen test window was made." );
    if ( !window ) return;

    int target = names.size() < 2 ? showing : ( showing == 0 ? 1 : 0 );
    wstring problem = move_window_to_desktop( window, names[target] );
    check( problem.find( L"not on any desktop yet" ) != wstring::npos,
           "Moving a window that was never shown returns a problem saying it is not on any desktop yet." );
    check( current_desktop_index() == showing, "Moving a window that was never shown leaves the same desktop showing." );

    ShowWindow( window, SW_SHOWNOACTIVATE );
    for ( int wait = 0; wait < 20 && desktop_of( window ) != showing; ++wait ) Sleep( 100 );
    check( desktop_of( window ) == showing,
           "Within 2 seconds of being shown, the once hidden window is reported on the desktop showing." );

    if ( names.size() < 2 ) {
        check( true, "There is only one desktop, so moving the once hidden window to another desktop is skipped." );
    } else {
        wstring shown_problem = move_window_to_desktop( window, names[target] );
        check( shown_problem.empty(), "Once shown, the once hidden window moves to another desktop with no problem." );
        check( desktop_of( window ) == target, "After that move, the once hidden window is reported on the other desktop." );
        check( current_desktop_index() == showing, "Moving the once hidden window leaves the same desktop showing." );

        wstring back_problem = move_window_to_desktop( window, names[showing] );
        check( back_problem.empty(), "Moving the once hidden window back to the desktop showing returns no problem." );
        check( desktop_of( window ) == showing, "The once hidden window is back on the desktop showing." );
    }

    DestroyWindow( window );
    check( current_desktop_index() == showing, "The desktop showing is the same after the checks on a window never shown." );
}

void check_moving_windows() {
    int showing = current_desktop_index();
    vector<wstring> names = desktop_names();
    if ( showing < 0 || showing >= (int)names.size() ) {
        check( false, "The desktop showing is known, so the window moving checks can run." );
        return;
    }
    HWND window = make_window( unique_title( L"Crammy desktop setups check" ) );
    check( window != nullptr, "The off-screen test window for moving was made." );
    for ( int wait = 0; wait < 20 && desktop_of( window ) != showing; ++wait ) Sleep( 100 );
    check( desktop_of( window ) == showing, "The new test window starts on the desktop showing." );

    check_move_to_same_desktop( window, names, showing );
    check_move_to_unknown_desktop( window, showing );
    check_move_to_other_desktop( window, names, showing );

    DestroyWindow( window );
    check_not_a_window( showing );
    check_window_never_shown( names, showing );
    check( current_desktop_index() == showing, "The desktop showing is the same after all the window moving checks." );
}

void check_notepad_command() {
    wstring path = L"C:\\Some folder\\ECE 150 notes.txt";
    check( notepad_command( path ) ==
               quoted( program_file( program::notepad_pp ) ) + L" -multiInst -nosession " + quoted( path ),
           "notepad_command is the quoted Notepad++ path, then -multiInst -nosession, then the quoted file." );

    wstring plain_path = L"C:\\notes.txt";
    check( notepad_command( plain_path ) ==
               quoted( program_file( program::notepad_pp ) ) + L" -multiInst -nosession " + quoted( plain_path ),
           "notepad_command has the same form for a path without spaces." );
}

vector<HWND> notepad_windows() {
    vector<HWND> windows;
    HWND window = nullptr;
    while ( ( window = FindWindowExW( nullptr, window, L"Notepad++", nullptr ) ) != nullptr )
        windows.push_back( window );
    return windows;
}

// "C:\path\file.txt - Notepad++", with a leading '*' when the file has unsaved changes
wstring file_in_title( HWND window ) {
    wstring title = title_of( window );
    wstring ending = L" - Notepad++";
    if ( ends_with( title, ending ) )
        title = title.substr( 0, title.size() - ending.size() );
    if ( !title.empty() && title[0] == L'*' )
        title = title.substr( 1 );
    return title;
}

struct scratch_notepad {
    PROCESS_INFORMATION info{};
    HWND window = nullptr;
};

// A separate Notepad++, started minimized without taking the focus
scratch_notepad start_notepad( const vector<wstring> &paths ) {
    scratch_notepad started;
    wstring command = quoted( program_file( program::notepad_pp ) ) + L" -multiInst -nosession -noPlugin";
    for ( const wstring &path : paths ) command += L" " + quoted( path );
    STARTUPINFOW startup{ sizeof( startup ) };
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_SHOWMINNOACTIVE;
    if ( !CreateProcessW( nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &started.info ) )
        return started;
    WaitForInputIdle( started.info.hProcess, 5000 );
    for ( int wait = 0; wait < 50 && !started.window; ++wait ) {
        Sleep( 100 );
        for ( HWND each : notepad_windows() ) {
            DWORD process_id = 0;
            GetWindowThreadProcessId( each, &process_id );
            if ( process_id == started.info.dwProcessId ) started.window = each;
        }
    }
    return started;
}

int tab_count( HWND window ) {
    return int( SendMessageW( window, WM_USER + 1000 + 7, 0, 1 ) );
}

bool reaches_tab_count( HWND window, int count ) {
    for ( int wait = 0; wait < 30; ++wait ) {
        if ( window && tab_count( window ) == count ) return true;
        Sleep( 100 );
    }
    return false;
}

void stop_notepad( scratch_notepad &started ) {
    if ( !started.info.hProcess ) return;
    if ( started.window && still_ours( started.window, started.info.dwProcessId ) )
        PostMessageW( started.window, WM_CLOSE, 0, 0 );
    if ( WaitForSingleObject( started.info.hProcess, 5000 ) != WAIT_OBJECT_0 )
        TerminateProcess( started.info.hProcess, 0 );
    CloseHandle( started.info.hThread );
    CloseHandle( started.info.hProcess );
    started.info = {};
}

wstring notes_list_path() {
    wchar_t buffer[MAX_PATH * 4] = {};
    GetModuleFileNameW( nullptr, buffer, MAX_PATH * 4 );
    return join_path( parent_folder( buffer ), L"notes opened.txt" );
}

void check_closing_old_notes() {
    wstring folder = join_path( expand_variables( L"%TEMP%" ), L"crammy desktop setups check" );
    CreateDirectoryW( folder.c_str(), nullptr );
    wstring closing_notes = join_path( folder, L"closing notes.txt" );
    wstring kept_notes = join_path( folder, L"kept notes.txt" );
    wstring several_old = join_path( folder, L"several old notes.txt" );
    wstring several_own = join_path( folder, L"several your own.txt" );
    for ( const wstring &path : { closing_notes, kept_notes, several_old, several_own } )
        write_text_file( path, L"notes\r\n" );

    // Every file the owner already has open is kept, so their own notes are never touched.
    vector<HWND> owner_windows = notepad_windows();
    vector<wstring> keep = { kept_notes };
    for ( HWND window : owner_windows ) {
        wstring path = file_in_title( window );
        if ( !path.empty() ) keep.push_back( path );
    }

    scratch_notepad closing = start_notepad( { closing_notes } );
    scratch_notepad kept = start_notepad( { kept_notes } );
    scratch_notepad several = start_notepad( { several_old, several_own } );
    bool all_started = closing.window && kept.window && several.window;
    check( all_started, "Three separate minimized Notepad++ windows start for the closing notes check." );

    if ( all_started ) {
        check( reaches_tab_count( closing.window, 1 ), "The first scratch Notepad++ opens with one file." );
        check( reaches_tab_count( kept.window, 1 ), "The second scratch Notepad++ opens with one file." );
        check( reaches_tab_count( several.window, 2 ), "The third scratch Notepad++ opens with two files." );

        remember_notes( closing_notes );
        remember_notes( kept_notes );
        remember_notes( several_old );
        close_old_notes( keep, nullptr );

        check( WaitForSingleObject( closing.info.hProcess, 5000 ) == WAIT_OBJECT_0,
               "The Notepad++ whose only file was remembered and not kept has ended within 5 seconds." );
        check( still_ours( kept.window, kept.info.dwProcessId ),
               "The Notepad++ whose only file is kept is still open." );
        check( still_ours( kept.window, kept.info.dwProcessId ) && tab_count( kept.window ) == 1,
               "The Notepad++ whose only file is kept still has exactly one file open." );
        check( still_ours( several.window, several.info.dwProcessId ),
               "The Notepad++ with several files stays open when one of its files closes." );
        check( still_ours( several.window, several.info.dwProcessId ) && tab_count( several.window ) == 1,
               "In the Notepad++ with several files, only the remembered tab closes and your own file stays." );

        bool owner_windows_open = true;
        for ( HWND window : owner_windows )
            if ( !IsWindow( window ) ) owner_windows_open = false;
        check( owner_windows_open, "Every Notepad++ window that was open before the check is still open." );

        wstring notes_list = read_text_file( notes_list_path() );
        check( contains_ci( notes_list, kept_notes ), "notes opened.txt still lists the kept notes file, which is open." );
        check( !contains_ci( notes_list, closing_notes ),
               "notes opened.txt no longer lists the notes file whose window closed." );
        check( !contains_ci( notes_list, several_old ),
               "notes opened.txt no longer lists the notes file whose tab closed." );
    }

    stop_notepad( closing );
    stop_notepad( kept );
    stop_notepad( several );
    for ( const wstring &path : { closing_notes, kept_notes, several_old, several_own } )
        DeleteFileW( path.c_str() );
    RemoveDirectoryW( folder.c_str() );
    check( !is_folder( folder ), "The scratch folder for the closing notes check is gone afterwards." );
}

void check_empty_command_line() {
    command_line parsed = parse_command_line( {} );
    check( parsed.action.empty() && parsed.list_title.empty() && parsed.setups.empty() && !parsed.show,
           "No words give an empty action, no list title, no setups and show off." );
    check( parsed.error.empty(), "No words give no error." );
}

void check_setup_examples() {
    command_line one = parse_command_line( { L"--setup", L"ECE 150: Exam", L"--pick", L"F24-F" } );
    check( one.error.empty(), "--setup with a title and one pick gives no error." );
    check( one.setups.size() == 1 && one.setups[0].title == L"ECE 150: Exam",
           "--setup \"ECE 150: Exam\" makes one request with that title." );
    check( one.setups.size() == 1 && one.setups[0].picks == vector<wstring>{ L"F24-F" },
           "--pick F24-F gives that request the one pick F24-F." );
    check( one.setups.size() == 1 && !one.setups[0].separate, "A request without --separate is not separate." );
    check( one.action.empty() && !one.show, "A setup request leaves the action empty and show off." );

    command_line two = parse_command_line( { L"--setup", L"Math 115: Exam", L"--pick", L"F23-F", L"F23-M", L"--separate" } );
    check( two.error.empty(), "--setup with two picks and --separate gives no error." );
    check( two.setups.size() == 1 && two.setups[0].picks == vector<wstring>{ L"F23-F", L"F23-M" },
           "--pick takes every following word that does not start with a dash." );
    check( two.setups.size() == 1 && two.setups[0].separate, "--separate after --setup sets separate on that request." );

    command_line three = parse_command_line(
        { L"--setup", L"A", L"--pick", L"x", L"--setup", L"B", L"--setup", L"C", L"--pick", L"y", L"z" } );
    check( three.error.empty(), "Three --setup requests give no error." );
    check( three.setups.size() == 3, "Three --setup words make three requests." );
    if ( three.setups.size() == 3 ) {
        check( three.setups[0].title == L"A" && three.setups[1].title == L"B" && three.setups[2].title == L"C",
               "The requests keep the order of their --setup words." );
        check( three.setups[0].picks == vector<wstring>{ L"x" }, "The first request gets only the pick after it." );
        check( three.setups[1].picks.empty(), "A request with no --pick after it has no picks." );
        check( three.setups[2].picks == vector<wstring>{ L"y", L"z" }, "The last request gets the picks after it." );
    }

    command_line shouting = parse_command_line( { L"--SETUP", L"A", L"--PICK", L"x" } );
    check( shouting.error.empty() && shouting.setups.size() == 1 && shouting.setups[0].title == L"A" &&
               shouting.setups[0].picks == vector<wstring>{ L"x" },
           "--SETUP and --PICK in capitals work the same as in small letters." );

    command_line later = parse_command_line( { L"--setup", L"A", L"--setup", L"B", L"--separate" } );
    check( later.setups.size() == 2 && !later.setups[0].separate && later.setups[1].separate,
           "--separate sets separate only on the latest --setup." );

    command_line twice = parse_command_line( { L"--setup", L"A", L"--pick", L"x", L"--pick", L"y" } );
    check( twice.setups.size() == 1 && twice.setups[0].picks == vector<wstring>{ L"x", L"y" },
           "A second --pick adds its words to the picks of the same request." );

    command_line stop = parse_command_line( { L"--setup", L"A", L"--pick", L"x", L"y", L"--show" } );
    check( stop.setups.size() == 1 && stop.setups[0].picks == vector<wstring>{ L"x", L"y" },
           "The picks stop at the next word that starts with a dash." );
    check( stop.show && stop.error.empty(), "--show after the picks sets show." );
}

void check_command_line_errors() {
    command_line pick_alone = parse_command_line( { L"--pick", L"x" } );
    check( pick_alone.error == L"--pick needs a --setup before it.", "--pick with no --setup before it gives the --pick error." );

    command_line separate_alone = parse_command_line( { L"--separate" } );
    check( separate_alone.error == L"--separate needs a --setup before it.",
           "--separate with no --setup before it gives the --separate error." );

    command_line setup_alone = parse_command_line( { L"--setup" } );
    check( setup_alone.error == L"--setup needs a setup name after it.",
           "--setup at the end of the words gives the missing setup name error." );

    command_line setup_dash = parse_command_line( { L"--setup", L"--show" } );
    check( setup_dash.error == L"--setup needs a setup name after it.",
           "--setup followed by a word starting with a dash gives the missing setup name error." );

    command_line wobble = parse_command_line( { L"--wobble" } );
    check( wobble.error == L"Unknown option: --wobble", "An unknown option gives \"Unknown option: \" and the word." );

    command_line mixed = parse_command_line( { L"--Wobble" } );
    check( mixed.error == L"Unknown option: --Wobble", "The unknown option error shows the word as it was given." );

    command_line stray = parse_command_line( { L"hello" } );
    check( stray.error == L"Unknown option: hello", "A stray word that is not an option is an unknown option." );

    command_line first_unknown = parse_command_line( { L"--wobble", L"--pick", L"x" } );
    check( first_unknown.error == L"Unknown option: --wobble", "When an unknown option comes first, its error is the one kept." );

    command_line first_pick = parse_command_line( { L"--pick", L"x", L"--wobble" } );
    check( first_pick.error == L"--pick needs a --setup before it.", "When the --pick error comes first, it is the one kept." );
}

void check_actions() {
    command_line list = parse_command_line( { L"--list", L"ECE 150: Exam" } );
    check( list.action == L"list", "--list sets the action to list." );
    check( list.list_title == L"ECE 150: Exam", "--list takes the next word as the list title." );
    check( list.setups.empty() && list.error.empty(), "--list with a title makes no requests and gives no error." );

    command_line list_alone = parse_command_line( { L"--list" } );
    check( list_alone.action == L"list" && list_alone.list_title.empty() && list_alone.error.empty(),
           "--list with nothing after it sets the action and leaves the list title empty." );

    command_line list_dash = parse_command_line( { L"--list", L"--show" } );
    check( list_dash.action == L"list" && list_dash.list_title.empty(),
           "--list followed by a word starting with a dash leaves the list title empty." );
    check( list_dash.show && list_dash.error.empty(), "The word after --list that starts with a dash is still read as an option." );

    check( parse_command_line( { L"--check" } ).action == L"check", "--check sets the action to check." );
    check( parse_command_line( { L"--edit" } ).action == L"edit", "--edit sets the action to edit." );
    check( parse_command_line( { L"--background" } ).action == L"background", "--background sets the action to background." );
    check( parse_command_line( { L"--panel" } ).action == L"panel", "--panel sets the action to panel." );
    check( parse_command_line( { L"--Panel" } ).action == L"panel", "Action options are matched without regard to case." );

    command_line show = parse_command_line( { L"--show" } );
    check( show.show && show.error.empty(), "--show sets show." );
    check( parse_command_line( { L"--SHOW" } ).show, "--SHOW in capitals sets show." );
}

planned_window planned( const wstring &side, const wstring &desktop ) {
    planned_window window;
    window.side = side;
    window.app = program::pdfcraft;
    window.paths = { L"C:\\notes.pdf" };
    window.desktop = desktop;
    return window;
}

bool same_rect( const RECT &a, const RECT &b ) {
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

bool same_targets( const vector<RECT> &targets, const vector<RECT> &expected ) {
    if ( targets.size() != expected.size() ) return false;
    for ( size_t i = 0; i < targets.size(); ++i )
        if ( !same_rect( targets[i], expected[i] ) ) return false;
    return true;
}

void check_window_targets() {
    RECT screen = { 0, 0, 1000, 600 };

    check( window_targets( {}, screen ).empty(), "An empty plan gives no targets." );

    check( same_targets( window_targets( { planned( L"left", L"" ), planned( L"right", L"" ) }, screen ),
                         { { 0, 0, 500, 600 }, { 500, 0, 1000, 600 } } ),
           "A left and a right window with no desktop get the left half and the right half." );

    check( same_targets( window_targets( { planned( L"full", L"105" ), planned( L"full", L"192" ) }, screen ),
                         { { 0, 0, 1000, 600 }, { 0, 0, 1000, 600 } } ),
           "Two full windows on different desktops each get the whole screen." );

    check( same_targets( window_targets( { planned( L"left", L"150" ), planned( L"left", L"150" ), planned( L"left", L"190" ) },
                                         screen ),
                         { { 0, 0, 500, 300 }, { 0, 300, 500, 600 }, { 0, 0, 500, 600 } } ),
           "Two left windows on desktop 150 split the left half into rows, and the left window on 190 gets the whole half." );

    check( same_targets( window_targets( { planned( L"right", L"150" ), planned( L"right", L"150" ), planned( L"right", L"150" ) },
                                         screen ),
                         { { 500, 0, 1000, 200 }, { 500, 200, 1000, 400 }, { 500, 400, 1000, 600 } } ),
           "Three right windows on the same desktop split the right half into three rows, top to bottom." );

    check( same_targets( window_targets( { planned( L"left", L"A" ), planned( L"left", L"a" ) }, screen ),
                         { { 0, 0, 500, 300 }, { 0, 300, 500, 600 } } ),
           "Desktop names A and a are the same desktop, so their left windows share rows." );

    RECT second_screen = { 100, 50, 1101, 651 };
    check( same_targets( window_targets( { planned( L"left", L"" ) }, second_screen ), { { 100, 50, 600, 651 } } ),
           "On the screen from 100,50 to 1101,651 a left window runs from 100 to 600." );
    check( same_targets( window_targets( { planned( L"right", L"" ) }, second_screen ), { { 600, 50, 1101, 651 } } ),
           "On the screen from 100,50 to 1101,651 a right window runs from 600 to 1101." );

    RECT odd_width = { 0, 0, 999, 600 };
    check( same_targets( window_targets( { planned( L"left", L"" ), planned( L"right", L"" ) }, odd_width ),
                         { { 0, 0, 499, 600 }, { 499, 0, 999, 600 } } ),
           "On a screen 999 wide the halves meet at 499, the width halved with integer division." );

    RECT odd_height = { 0, 0, 1000, 601 };
    check( same_targets( window_targets( { planned( L"left", L"150" ), planned( L"left", L"150" ), planned( L"left", L"150" ) },
                                         odd_height ),
                         { { 0, 0, 500, 200 }, { 0, 200, 500, 400 }, { 0, 400, 500, 601 } } ),
           "On a screen 601 high three rows are 200 high and the last row runs to the bottom of the screen." );

    check( same_targets( window_targets( { planned( L"left", L"150" ), planned( L"right", L"150" ), planned( L"left", L"150" ) },
                                         screen ),
                         { { 0, 0, 500, 300 }, { 500, 0, 1000, 600 }, { 0, 300, 500, 600 } } ),
           "Targets come back in plan order, and rows on a side go top to bottom in plan order." );

    check( same_targets( window_targets( { planned( L"left", L"" ), planned( L"left", L"150" ) }, screen ),
                         { { 0, 0, 500, 600 }, { 0, 0, 500, 600 } } ),
           "A left window with no desktop and a left window on desktop 150 each get the whole left half." );

    check( same_targets( window_targets( { planned( L"full", L"150" ), planned( L"full", L"150" ) }, screen ),
                         { { 0, 0, 1000, 300 }, { 0, 300, 1000, 600 } } ),
           "Two full windows on the same desktop split the whole screen into two rows." );
}

// The lines of a text, with any carriage return at the end of a line taken off.
vector<wstring> lines_of( const wstring &text ) {
    vector<wstring> lines;
    size_t start = 0;
    while ( start <= text.size() ) {
        size_t end = text.find( L'\n', start );
        if ( end == wstring::npos ) end = text.size();
        wstring line = text.substr( start, end - start );
        if ( !line.empty() && line.back() == L'\r' ) line.pop_back();
        lines.push_back( line );
        start = end + 1;
    }
    return lines;
}

bool has_line( const wstring &text, const wstring &wanted ) {
    for ( const wstring &line : lines_of( text ) )
        if ( line == wanted ) return true;
    return false;
}

// True when the line right after the first line equal to first is equal to second.
bool line_follows( const wstring &text, const wstring &first, const wstring &second ) {
    vector<wstring> lines = lines_of( text );
    for ( size_t i = 0; i + 1 < lines.size(); ++i )
        if ( lines[i] == first ) return lines[i + 1] == second;
    return false;
}

bool has_split_line( const wstring &text ) {
    for ( const wstring &line : lines_of( text ) )
        if ( line.compare( 0, 5, L"split" ) == 0 ) return true;
    return false;
}

planned_window planned_split( const wstring &side, const wstring &desktop, int split ) {
    planned_window window = planned( side, desktop );
    window.split = split;
    return window;
}

const wchar_t *split_text =
    L"[ECE 150]\n"
    L"root = C:\\Courses\\ECE 150\n"
    L"desktop = 150\n"
    L"split = 75\n"
    L"left = notes\n"
    L"\n"
    L"[ECE 150: Exam]\n"
    L"desktop = 151\n"
    L"split = 30\n"
    L"right = notes\n"
    L"\n"
    L"[ECE 150: Lecture]\n"
    L"right = notes\n";

const wchar_t *no_split_text =
    L"[ECE 150]\n"
    L"root = C:\\Courses\\ECE 150\n"
    L"desktop = 150\n"
    L"left = notes\n"
    L"\n"
    L"[ECE 150: Exam]\n"
    L"desktop = 151\n"
    L"right = notes\n";

void check_reading_split() {
    profile_book book = read_book( split_text );
    check( book.errors.empty(), "Profile text with split lines on a subject and a setup reads without errors." );
    check( book.profiles.size() == 1 && book.profiles[0].split == 75,
           "split = 75 under a subject heading gives that subject a split of 75." );
    check( book.profiles.size() == 1 && book.profiles[0].setups.size() == 2 && book.profiles[0].setups[0].split == 30,
           "split = 30 under a setup heading gives that setup a split of 30." );
    check( book.profiles.size() == 1 && book.profiles[0].setups.size() == 2 && book.profiles[0].setups[1].split == 0,
           "A setup without a split line has a split of 0 in its block." );

    profile_book bare = read_book( no_split_text );
    check( bare.errors.empty() && bare.profiles.size() == 1 && bare.profiles[0].split == 0,
           "A subject without a split line has a split of 0 in its block." );

    vector<setup> setups = all_setups( book );
    check( setups.size() == 2 && setups[0].split == 30,
           "In all_setups, a setup's own split of 30 wins over its subject's split of 75." );
    check( setups.size() == 2 && setups[1].split == 75,
           "In all_setups, a setup without a split line takes its subject's split of 75." );

    vector<setup> plain = all_setups( bare );
    check( plain.size() == 1 && plain[0].split == 50,
           "In all_setups, the split is 50 when neither the setup nor its subject has a split line." );

    vector<setup> lone = all_setups( read_book( L"[ECE 198]\nroot = C:\\Courses\\ECE 198\nsplit = 60\nfull = notes\n" ) );
    check( lone.size() == 1 && lone[0].split == 60,
           "In all_setups, a subject with no setups opens with its own split of 60." );

    profile_book spaced = read_book( L"[X]\nroot = C:\\X\nsplit =    75   \nleft = notes\n" );
    check( spaced.errors.empty() && spaced.profiles.size() == 1 && spaced.profiles[0].split == 75,
           "Spaces around the number in a split line are allowed and the number is still read." );

    profile_book lowest = read_book( L"[X]\nroot = C:\\X\nsplit = 10\nleft = notes\n" );
    check( lowest.errors.empty() && lowest.profiles.size() == 1 && lowest.profiles[0].split == 10,
           "split = 10, the smallest allowed, is read without an error." );

    profile_book highest = read_book( L"[X]\nroot = C:\\X\nsplit = 90\nleft = notes\n" );
    check( highest.errors.empty() && highest.profiles.size() == 1 && highest.profiles[0].split == 90,
           "split = 90, the largest allowed, is read without an error." );
}

void check_bad_split() {
    struct bad_case {
        const wchar_t *line;
        const char *what;
    };
    const bad_case cases[] = {
        { L"split =", "An empty split value" },
        { L"split = abc", "The split value abc" },
        { L"split = 9", "The split value 9" },
        { L"split = 91", "The split value 91" },
        { L"split = 75%", "The split value 75%" },
        { L"split = 50.5", "The split value 50.5" },
    };
    for ( const bad_case &each : cases ) {
        profile_book book = read_book( wstring( L"[X]\nroot = C:\\X\n" ) + each.line + L"\nleft = notes\n" );
        check( book.errors.size() == 1 && book.errors[0] == L"line 3: split must be a whole number from 10 to 90",
               ( std::string( each.what ) + " on line 3 of a subject gives the line 3 split error." ).c_str() );
        check( book.profiles.size() == 1 && book.profiles[0].split == 0,
               ( std::string( each.what ) + " leaves the subject's split at 0." ).c_str() );

        profile_book in_setup =
            read_book( wstring( L"[X]\nroot = C:\\X\nleft = notes\n\n[X: A]\n" ) + each.line + L"\nright = notes\n" );
        check( in_setup.errors.size() == 1 && in_setup.errors[0] == L"line 6: split must be a whole number from 10 to 90",
               ( std::string( each.what ) + " on line 6 of a setup gives the line 6 split error." ).c_str() );
        check( in_setup.profiles.size() == 1 && in_setup.profiles[0].setups.size() == 1 &&
                   in_setup.profiles[0].setups[0].split == 0,
               ( std::string( each.what ) + " leaves the setup's split at 0." ).c_str() );
    }
}

void check_writing_split() {
    profile_book book = read_book( split_text );
    wstring written = write_book( book );
    check( line_follows( written, L"desktop = 150", L"split = 75" ),
           "write_book writes split = 75 on the line after the subject's desktop line." );
    check( line_follows( written, L"desktop = 151", L"split = 30" ),
           "write_book writes split = 30 on the line after the setup's desktop line." );

    profile_book again = read_book( written );
    check( again.errors.empty() && again.profiles.size() == 1 && again.profiles[0].split == 75 &&
               again.profiles[0].setups.size() == 2 && again.profiles[0].setups[0].split == 30 &&
               again.profiles[0].setups[1].split == 0,
           "Reading what write_book wrote gives back the same split values." );
    check( write_book( again ) == written, "Writing a book with split lines a second time gives the same text." );

    wstring written_bare = write_book( read_book( no_split_text ) );
    check( !has_split_line( written_bare ), "write_book writes no split line for a file that had none." );
    check( write_book( read_book( written_bare ) ) == written_bare,
           "A file without split lines reads and writes back unchanged." );

    check( has_line( written_bare, L"#   split = 75                   the left side takes 75% of the width (default 50)" ),
           "The comment block at the top of a written file has the split line." );
}

void check_split_targets() {
    RECT screen = { 0, 0, 1000, 600 };

    check( same_targets( window_targets( { planned_split( L"left", L"", 75 ), planned_split( L"right", L"", 75 ) }, screen ),
                         { { 0, 0, 750, 600 }, { 750, 0, 1000, 600 } } ),
           "With split 75 the left window gets the left 750 and the right window the rest." );

    check( same_targets( window_targets( { planned_split( L"left", L"", 25 ), planned_split( L"right", L"", 25 ) }, screen ),
                         { { 0, 0, 250, 600 }, { 250, 0, 1000, 600 } } ),
           "With split 25 the left window gets the left 250 and the right window the rest." );

    check( same_targets( window_targets( { planned_split( L"full", L"", 75 ) }, screen ), { { 0, 0, 1000, 600 } } ),
           "A full window with split 75 still gets the whole screen." );

    check( same_targets( window_targets( { planned_split( L"right", L"150", 75 ), planned_split( L"right", L"150", 75 ) },
                                         screen ),
                         { { 750, 0, 1000, 300 }, { 750, 300, 1000, 600 } } ),
           "Two right windows on desktop 150 with split 75 split the right 250 into two rows." );

    check( same_targets( window_targets( { planned_split( L"left", L"150", 75 ), planned_split( L"left", L"190", 50 ) },
                                         screen ),
                         { { 0, 0, 750, 600 }, { 0, 0, 500, 600 } } ),
           "Each window uses its own split: 75 on desktop 150 and 50 on desktop 190." );

    RECT odd_width = { 0, 0, 1001, 600 };
    check( same_targets( window_targets( { planned_split( L"left", L"", 75 ) }, odd_width ), { { 0, 0, 750, 600 } } ),
           "On a screen 1001 wide a left window with split 75 runs to 750, with integer arithmetic." );
}

void check_split() {
    check_reading_split();
    check_bad_split();
    check_writing_split();
    check_split_targets();
}

void check_viewer_commands() {
    wstring pdfcraft_start = quoted( program_file( program::pdfcraft ) ) + L" --new-window --mode read";

    check( pdfcraft_command( { L"a.pdf", L"b c.pdf" }, L"" ) == pdfcraft_start + L" \"a.pdf\" \"b c.pdf\"",
           "pdfcraft_command on a.pdf and b c.pdf with no control file is the quoted PdfCraft path, "
           "--new-window --mode read, then each PDF quoted in order." );

    check( pdfcraft_command( { L"a.pdf" }, L"C:\\t\\ctl.txt" ) ==
               pdfcraft_start + L" --control \"C:\\t\\ctl.txt\" \"a.pdf\"",
           "pdfcraft_command on a.pdf with the control file C:\\t\\ctl.txt puts --control and the quoted file "
           "after --mode read and before the PDF." );

    check( pdfcraft_command( { L"a.pdf" }, L"" ) == pdfcraft_start + L" \"a.pdf\"",
           "pdfcraft_command on one PDF with no control file is the quoted PdfCraft path, "
           "--new-window --mode read, then the quoted PDF." );

    check( pdfcraft_command( { L"a.pdf" }, L"" ).find( L"--control" ) == wstring::npos,
           "pdfcraft_command on one PDF with no control file has no --control in it." );

    check( acrobat_command( L"a b.pdf" ) == quoted( program_file( program::acrobat ) ) + L" /n /A \"view=FitH\" \"a b.pdf\"",
           "acrobat_command on a b.pdf is the quoted Acrobat path, /n /A \"view=FitH\", then the quoted PDF." );
    check( pdfcraft_command( { L"a.pdf" }, L"" ).find( L"--zoom" ) == wstring::npos &&
               pdfcraft_command( { L"a.pdf" }, L"C:\\t\\ctl.txt" ).find( L"--zoom" ) == wstring::npos,
           "pdfcraft_command never passes --zoom, so PdfCraft fits the page width." );
}

} // namespace

void test_desktop_setups() {
    int showing_at_start = current_desktop_index();
    register_window_class();
    check_moving_windows();
    check_notepad_command();
    check_closing_old_notes();
    check_empty_command_line();
    check_setup_examples();
    check_command_line_errors();
    check_actions();
    check_window_targets();
    check_split();
    check_viewer_commands();
    check( current_desktop_index() == showing_at_start, "The desktop showing is the same after all the desktop setups checks." );
}
