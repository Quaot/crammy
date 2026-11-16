// Written from tests\specs\background_log.md.

#include <windows.h>

#include <fstream>
#include <iterator>

#include "check.h"
#include "../src/background.h"
#include "../src/text.h"

namespace {

wstring test_program_folder() {
    wchar_t path[MAX_PATH * 4];
    DWORD length = GetModuleFileNameW( nullptr, path, MAX_PATH * 4 );
    return parent_folder( wstring( path, length ) );
}

// The file's text exactly as written, with a byte order mark taken off if there is one
wstring raw_text( const wstring &path ) {
    std::ifstream file( path.c_str(), std::ios::binary );
    if ( !file ) return L"<no file>";
    std::string bytes( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
    if ( bytes.size() >= 2 && bytes[0] == '\xFF' && bytes[1] == '\xFE' ) {
        wstring text;
        for ( size_t at = 2; at + 1 < bytes.size(); at += 2 )
            text += wchar_t( (unsigned char)bytes[at] | ( (unsigned char)bytes[at + 1] << 8 ) );
        return text;
    }
    if ( bytes.size() >= 3 && bytes.compare( 0, 3, "\xEF\xBB\xBF" ) == 0 ) bytes.erase( 0, 3 );
    return from_utf8( bytes );
}

SYSTEMTIME make_time( WORD year, WORD month, WORD day, WORD hour, WORD minute, WORD second, WORD weekday,
                      WORD milliseconds ) {
    SYSTEMTIME at{};
    at.wYear = year;
    at.wMonth = month;
    at.wDay = day;
    at.wHour = hour;
    at.wMinute = minute;
    at.wSecond = second;
    at.wDayOfWeek = weekday;
    at.wMilliseconds = milliseconds;
    return at;
}

void check_window_ignores_close() {
    HINSTANCE instance = GetModuleHandleW( nullptr );
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = background_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"CrammyLogCheck";
    bool registered = RegisterClassW( &window_class ) != 0;
    check( registered, "the CrammyLogCheck class registers with background_window_proc" );

    HWND window = CreateWindowExW( WS_EX_TOOLWINDOW, L"CrammyLogCheck", L"", WS_POPUP, -32000, -32000, 1, 1, nullptr,
                                   nullptr, instance, nullptr );
    check( window != nullptr, "a hidden window of that class is made" );
    if ( !window ) {
        UnregisterClassW( L"CrammyLogCheck", instance );
        return;
    }
    check( !IsWindowVisible( window ), "the log check window is never shown" );

    SendMessageW( window, WM_CLOSE, 0, 0 );
    check( IsWindow( window ), "the hidden window stays after WM_CLOSE" );

    LRESULT ended = SendMessageW( window, WM_ENDSESSION, TRUE, 0 );
    check( ended == 0, "WM_ENDSESSION returns 0" );
    check( IsWindow( window ), "the hidden window stays after WM_ENDSESSION" );

    DestroyWindow( window );
    check( !IsWindow( window ), "DestroyWindow removes the hidden window" );
    MSG message;
    bool found_quit = PeekMessageW( &message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE ) != 0;
    check( found_quit, "destroying the hidden window posts a quit message, which the test removes" );

    UnregisterClassW( L"CrammyLogCheck", instance );
}

void check_log_lines() {
    SYSTEMTIME started_at = make_time( 2026, 11, 5, 8, 1, 9, 4, 0 );
    check( log_line( started_at, L"started" ) == L"2026-11-05 08:01:09  started",
           "a log line has the date and time with leading zeros, two spaces, then the text" );

    SYSTEMTIME odd_parts = make_time( 2026, 11, 5, 8, 1, 9, 0, 987 );
    check( log_line( odd_parts, L"started" ) == L"2026-11-05 08:01:09  started",
           "the milliseconds and weekday do not change the log line" );

    SYSTEMTIME year_end = make_time( 2026, 12, 31, 23, 59, 59, 4, 0 );
    check( log_line( year_end, L"" ) == L"2026-12-31 23:59:59  ", "an empty text still gives the time and two spaces" );

    check( crash_text( 0xC0000005, 0x1A2B3 ) == L"crashed with exception 0xC0000005 at crammy.exe+0x1A2B3",
           "a crash is written with upper case hexadecimal code and offset" );
    check( crash_text( 0xE06D7363, 0 ) == L"crashed with exception 0xE06D7363 at crammy.exe+0x0",
           "an offset of zero is written as 0x0" );
}

void check_appending( const wstring &folder ) {
    wstring path = join_path( folder, L"log.txt" );
    DeleteFileW( path.c_str() );

    append_log( path, L"one", 3 );
    check( is_file( path ), "append_log makes the file when there is none" );
    check( raw_text( path ) == L"one\r\n", "the new file holds the first line and a line break" );

    append_log( path, L"two", 3 );
    append_log( path, L"three", 3 );
    check( raw_text( path ) == L"one\r\ntwo\r\nthree\r\n", "later lines are added to the end in order" );

    append_log( path, L"four", 3 );
    check( raw_text( path ) == L"two\r\nthree\r\nfour\r\n", "with keep 3 the oldest line goes when a fourth is added" );

    append_log( path, L"five", 1 );
    check( raw_text( path ) == L"five\r\n", "with keep 1 only the newest line is left" );

    DeleteFileW( path.c_str() );

    wstring missing_folder = join_path( folder, L"no such folder" );
    wstring missing_path = join_path( missing_folder, L"log.txt" );
    append_log( missing_path, L"one", 3 );
    check( !is_file( missing_path ) && !is_folder( missing_folder ),
           "a path in a missing folder writes nothing and does not crash" );
}

void check_real_log_path() {
    wstring expected = join_path( test_program_folder(), L"background log.txt" );
    check( lower( background_log_path() ) == lower( expected ),
           "the real log is background log.txt in the running program's folder" );
}

}  // namespace

void test_background_log() {
    wstring folder = join_path( expand_variables( L"%TEMP%" ), L"crammy log checks" );
    CreateDirectoryW( folder.c_str(), nullptr );

    check_window_ignores_close();
    check_log_lines();
    check_appending( folder );
    check_real_log_path();

    DeleteFileW( join_path( folder, L"log.txt" ).c_str() );
    RemoveDirectoryW( join_path( folder, L"no such folder" ).c_str() );
    RemoveDirectoryW( folder.c_str() );
}
