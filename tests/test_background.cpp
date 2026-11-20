// Written from tests\specs\background.md.

#include <windows.h>
#include <objbase.h>

#include "check.h"
#include "../src/background.h"
#include "../src/text.h"
#include "../src/timetable.h"

namespace {

int toggle_count = 0;
int next_count = 0;

LRESULT CALLBACK counting_proc( HWND window, UINT message, WPARAM w_param, LPARAM l_param ) {
    if ( message == show_panel_message() ) {
        toggle_count++;
        return 0;
    }
    if ( message == next_message() ) {
        next_count++;
        return 0;
    }
    return DefWindowProcW( window, message, w_param, l_param );
}

void pump_messages() {
    MSG message;
    while ( PeekMessageW( &message, nullptr, 0, 0, PM_REMOVE ) ) {
        TranslateMessage( &message );
        DispatchMessageW( &message );
    }
}

void check_sending_the_message() {
    if ( FindWindowW( L"CrammyBackground", nullptr ) ) {
        check( true, "A real Crammy is running in the background, so the message checks are skipped." );
        return;
    }

    check( !background_is_running(), "No background window is reported before one exists." );
    check( !ask_running_crammy_to_toggle_panel(), "Asking a Crammy that is not running to toggle the panel returns false." );
    check( show_panel_message() != 0, "The show panel message is not zero." );
    check( show_panel_message() == show_panel_message(), "The show panel message is the same on every call." );
    check( next_message() != 0, "The next message is not zero." );
    check( next_message() == next_message(), "The next message is the same on every call." );
    check( next_message() != show_panel_message(), "The next message differs from the show panel message." );
    check( !ask_running_crammy_for_next(), "Asking a Crammy that is not running for Next returns false." );

    HINSTANCE instance = GetModuleHandleW( nullptr );
    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = counting_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"CrammyBackground";
    RegisterClassW( &window_class );
    HWND window = CreateWindowExW( 0, L"CrammyBackground", L"", WS_OVERLAPPED, 0, 0, 100, 100, nullptr, nullptr, instance, nullptr );
    check( window != nullptr, "The test made its hidden background window." );

    toggle_count = 0;
    check( background_is_running(), "A background window is reported once one exists." );
    check( ask_running_crammy_to_toggle_panel(), "Asking a running Crammy to toggle the panel returns true." );
    pump_messages();
    check( toggle_count == 1, "The background window received the toggle message once." );
    ask_running_crammy_to_toggle_panel();
    ask_running_crammy_to_toggle_panel();
    pump_messages();
    check( toggle_count == 3, "Two more requests bring the count to three." );

    toggle_count = 0;
    next_count = 0;
    check( ask_running_crammy_for_next(), "Asking a running Crammy for Next returns true." );
    pump_messages();
    check( next_count == 1, "The background window received the next message once." );
    check( toggle_count == 0, "Asking for Next does not send the show panel message." );

    DestroyWindow( window );
    check( !background_is_running(), "No background window is reported after the test destroys it." );
    check( !ask_running_crammy_to_toggle_panel(), "Asking after the window is gone returns false." );
    UnregisterClassW( L"CrammyBackground", instance );
}

void check_the_title() {
    check( background_title( L"ECE 150 lec \u00B7 PSE 5353 \u00B7 in 5m", true ) == L"ECE 150 lec \u00B7 PSE 5353 \u00B7 in 5m",
           "The title is the line unchanged when Next can act." );
    check( background_title( L"GENE 119 sem \u00B7 STC 0050 \u00B7 10:30", false ) == L"-GENE 119 sem \u00B7 STC 0050 \u00B7 10:30",
           "The title has a dash in front when Next would do nothing." );
    check( background_title( L"No classes", false ) == L"-No classes", "No classes gets a dash in front when Next cannot act." );
}

void check_the_title_with_the_desktop() {
    check( taskbar_title( L"150", L"ECE 150 Lecture\nPSE 5353 · in 5m", true ) == L"150\tECE 150 Lecture\nPSE 5353 · in 5m",
           "The taskbar title is the desktop name, a tab and the line when Next can act." );
    check( taskbar_title( L"Main", L"GENE 119 Seminar\nSTC 0050 · 10:30", false ) == L"-Main\tGENE 119 Seminar\nSTC 0050 · 10:30",
           "The dash goes in front of the desktop name when Next would do nothing." );
    check( taskbar_title( L"", L"No classes", false ) == L"-No classes", "With no desktop name and Next unable to act, the title is the dashed line with no tab." );
    check( taskbar_title( L"", L"No classes", false ) == background_title( L"No classes", false ),
           "With no desktop name the title is the same as background_title." );
    check( taskbar_title( L"", L"MATH 117 Tutorial\nDWE 1515 · 08:30", true ) == L"MATH 117 Tutorial\nDWE 1515 · 08:30",
           "With no desktop name and Next able to act, the title is the line unchanged." );
    check( taskbar_title( L"Bakfiets", L"X", true ).compare( 0, 9, L"Bakfiets\t" ) == 0, "A desktop name of 12 characters or fewer is kept whole." );
    check( taskbar_title( L"Desktop number 12", L"X", true ) == L"Desktop num…\tX",
           "A longer desktop name is cut to its first 11 characters followed by an ellipsis." );
}

void check_already_open() {
    vector<wstring> one_file = { L"C:\\a\\L23.pdf" };
    vector<wstring> two_files = { L"C:\\a\\Slides.pdf", L"C:\\a\\Sum.pdf" };
    vector<wstring> no_files;

    check( all_already_open( one_file, { { L"c:\\A\\l23.PDF", true } } ),
           "A file shown by a living window with the path in other case is already open." );
    check( !all_already_open( one_file, { { L"c:\\A\\l23.PDF", false } } ),
           "A file whose window is gone is not already open." );
    check( !all_already_open( two_files, { { L"C:\\a\\Slides.pdf", true }, { L"C:\\a\\Sum.pdf", false } } ),
           "Two files with only the Slides window alive are not all open." );
    check( all_already_open( two_files, { { L"C:\\a\\Slides.pdf", true }, { L"C:\\a\\Sum.pdf", true } } ),
           "Two files with both windows alive are all open." );
    check( !all_already_open( no_files, { { L"C:\\a\\L23.pdf", true } } ), "An empty list of files is not already open, with a window." );
    check( !all_already_open( no_files, {} ), "An empty list of files is not already open, with no windows." );
    check( !all_already_open( one_file, {} ), "A file with no windows is not already open." );
}

void check_watch_end() {
    check( !watch_has_ended( { 2026, 11, 16 }, { 2026, 11, 16 } ), "A watch has not ended on the day it started." );
    check( watch_has_ended( { 2026, 11, 16 }, { 2026, 11, 17 } ), "A watch has ended the next day." );
    check( watch_has_ended( { 2026, 11, 30 }, { 2026, 12, 1 } ), "A watch has ended when the month changes." );
}

} // namespace

void test_background() {
    HRESULT started = CoInitialize( nullptr );
    check_sending_the_message();
    check_the_title();
    check_the_title_with_the_desktop();
    check_already_open();
    check_watch_end();
    if ( SUCCEEDED( started ) ) CoUninitialize();
}
