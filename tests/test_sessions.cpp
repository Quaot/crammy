// Written from tests\specs\sessions.md.

#include <windows.h>

#include <cstdint>
#include <string>

#include "check.h"
#include "../src/command_line.h"
#include "../src/desktops.h"
#include "../src/session.h"
#include "../src/tiling.h"

namespace {

// The lines of a text, with any carriage return at the end of a line taken off and any
// empty lines at the very end dropped.
vector<wstring> text_lines( const wstring &text ) {
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
    while ( !lines.empty() && lines.back().empty() ) lines.pop_back();
    return lines;
}

// True when every line feed in the text has a carriage return just before it.
bool every_line_ends_crlf( const wstring &text ) {
    for ( size_t i = 0; i < text.size(); ++i )
        if ( text[i] == L'\n' && ( i == 0 || text[i - 1] != L'\r' ) ) return false;
    return text.find( L'\n' ) != wstring::npos;
}

bool same_frame( const RECT &a, const RECT &b ) {
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

bool zero_frame( const RECT &frame ) {
    return same_frame( frame, RECT{ 0, 0, 0, 0 } );
}

bool same_window( const saved_window &a, const saved_window &b ) {
    return a.app == b.app && a.desktop == b.desktop && same_frame( a.frame, b.frame ) && a.paths == b.paths;
}

bool same_session( const saved_session &a, const saved_session &b ) {
    if ( a.asked != b.asked || a.windows.size() != b.windows.size() ) return false;
    for ( size_t i = 0; i < a.windows.size(); ++i )
        if ( !same_window( a.windows[i], b.windows[i] ) ) return false;
    return true;
}

saved_window saved( const wstring &app, const wstring &desktop, RECT frame, const vector<wstring> &paths ) {
    saved_window window;
    window.app = app;
    window.desktop = desktop;
    window.frame = frame;
    window.paths = paths;
    return window;
}

saved_session example_session() {
    saved_session session;
    session.asked = false;
    session.windows.push_back(
        saved( L"pdfcraft", L"150", { 0, 0, 1152, 1020 }, { L"C:\\Users\\x\\a.pdf", L"C:\\Users\\x\\b c.pdf" } ) );
    session.windows.push_back( saved( L"notepad++", L"", { -9, 0, 969, 1029 }, { L"C:\\notes.txt" } ) );
    return session;
}

// ---- 1. The session file ----

void check_writing_session() {
    wstring written = write_session( example_session() );
    vector<wstring> expected = {
        L"asked = no",
        L"",
        L"[window]",
        L"app = pdfcraft",
        L"desktop = 150",
        L"frame = 0 0 1152 1020",
        L"file = C:\\Users\\x\\a.pdf",
        L"file = C:\\Users\\x\\b c.pdf",
        L"",
        L"[window]",
        L"app = notepad++",
        L"desktop =",
        L"frame = -9 0 969 1029",
        L"file = C:\\notes.txt",
    };
    check( text_lines( written ) == expected, "write_session on the example session gives exactly the example text." );
    check( every_line_ends_crlf( written ), "write_session ends every line with \\r\\n." );

    saved_session asked = example_session();
    asked.asked = true;
    vector<wstring> asked_lines = text_lines( write_session( asked ) );
    check( !asked_lines.empty() && asked_lines[0] == L"asked = yes", "A session that was asked starts with asked = yes." );
    check( text_lines( written ).size() > 0 && text_lines( written )[0] == L"asked = no",
           "A session that was not asked starts with asked = no." );

    saved_session no_files;
    no_files.windows.push_back( saved( L"acrobat", L"192", { 1, 2, 3, 4 }, {} ) );
    vector<wstring> expected_no_files = {
        L"asked = no", L"", L"[window]", L"app = acrobat", L"desktop = 192", L"frame = 1 2 3 4",
    };
    check( text_lines( write_session( no_files ) ) == expected_no_files,
           "A window with no files still gets its block, with no file lines." );

    saved_session empty;
    check( text_lines( write_session( empty ) ) == vector<wstring>{ L"asked = no" },
           "An empty session is just the asked = no line." );
    saved_session empty_asked;
    empty_asked.asked = true;
    check( text_lines( write_session( empty_asked ) ) == vector<wstring>{ L"asked = yes" },
           "An empty session that was asked is just the asked = yes line." );

    saved_session negative;
    negative.windows.push_back( saved( L"pdfcraft", L"150", { -1920, -8, -1, -2 }, { L"C:\\a.pdf" } ) );
    vector<wstring> negative_lines = text_lines( write_session( negative ) );
    bool has_negative_frame = false;
    for ( const wstring &line : negative_lines )
        if ( line == L"frame = -1920 -8 -1 -2" ) has_negative_frame = true;
    check( has_negative_frame, "A frame with negative numbers is written as left, top, right, bottom with single spaces." );
}

void check_round_trips() {
    check( same_session( read_session( write_session( example_session() ) ), example_session() ),
           "Reading what write_session wrote for the example session gives the same session." );

    saved_session empty;
    check( same_session( read_session( write_session( empty ) ), empty ),
           "Reading what write_session wrote for an empty session gives an empty session." );

    saved_session mixed;
    mixed.asked = true;
    mixed.windows.push_back( saved( L"acrobat", L"Main desk", { -1920, -8, -1, 1032 }, { L"C:\\a b\\x.pdf" } ) );
    mixed.windows.push_back( saved( L"notepad++", L"", { 0, 0, 0, 0 }, {} ) );
    mixed.windows.push_back(
        saved( L"pdfcraft", L"115", { 960, 0, 1920, 1040 }, { L"C:\\1.pdf", L"C:\\2.pdf", L"C:\\folder with spaces\\3.pdf" } ) );
    check( same_session( read_session( write_session( mixed ) ), mixed ),
           "A session that was asked, with negative frames, a desktop name with a space, a window with no files and "
           "three files in order reads back the same." );
}

void check_reading_session() {
    saved_session empty = read_session( L"" );
    check( !empty.asked && empty.windows.empty(), "read_session on empty text gives an empty session that was not asked." );

    saved_session spaced = read_session( L"# a comment line\r\n"
                                         L"\r\n"
                                         L"   asked   =   yes   \r\n"
                                         L"\r\n"
                                         L"\r\n"
                                         L"[window]\r\n"
                                         L"# another comment\r\n"
                                         L"  app   =   acrobat  \r\n"
                                         L"\r\n"
                                         L"  desktop =   150  \r\n"
                                         L"  frame =   1 2 3 4  \r\n"
                                         L"  file =   C:\\s.pdf  \r\n" );
    check( spaced.asked, "read_session ignores spaces around the name and the value of the asked line." );
    check( spaced.windows.size() == 1, "read_session ignores blank lines and lines starting with #." );
    if ( spaced.windows.size() == 1 ) {
        check( spaced.windows[0].app == L"acrobat", "read_session ignores spaces around the app value." );
        check( spaced.windows[0].desktop == L"150", "read_session ignores spaces around the desktop value." );
        check( same_frame( spaced.windows[0].frame, RECT{ 1, 2, 3, 4 } ), "read_session ignores spaces around the frame value." );
        check( spaced.windows[0].paths == vector<wstring>{ L"C:\\s.pdf" }, "read_session ignores spaces around a file value." );
    }

    saved_session no_desktop = read_session( L"asked = no\r\n\r\n[window]\r\napp = notepad++\r\ndesktop =\r\nframe = 0 0 1 1\r\n" );
    check( no_desktop.windows.size() == 1 && no_desktop.windows[0].desktop.empty(),
           "desktop = with nothing after it reads as an empty desktop name." );

    saved_session odd = read_session( L"asked = no\r\n"
                                      L"this line means nothing\r\n"
                                      L"\r\n"
                                      L"[window]\r\n"
                                      L"app = pdfcraft\r\n"
                                      L"colour = blue\r\n"
                                      L"desktop = 150\r\n"
                                      L"frame = 5 6 7 8\r\n"
                                      L"file = C:\\a.pdf\r\n" );
    check( odd.windows.size() == 1 && odd.windows[0].app == L"pdfcraft" && odd.windows[0].desktop == L"150" &&
               same_frame( odd.windows[0].frame, RECT{ 5, 6, 7, 8 } ) &&
               odd.windows[0].paths == vector<wstring>{ L"C:\\a.pdf" },
           "read_session ignores lines it does not understand and still reads the rest." );

    saved_session bad_app = read_session( L"asked = no\r\n"
                                          L"\r\n"
                                          L"[window]\r\n"
                                          L"app = wordpad\r\n"
                                          L"desktop = 150\r\n"
                                          L"frame = 0 0 10 10\r\n"
                                          L"file = C:\\a.txt\r\n"
                                          L"\r\n"
                                          L"[window]\r\n"
                                          L"app = notepad++\r\n"
                                          L"desktop = 192\r\n"
                                          L"frame = 0 0 20 20\r\n"
                                          L"file = C:\\b.txt\r\n" );
    check( bad_app.windows.size() == 1 && bad_app.windows[0].app == L"notepad++" && bad_app.windows[0].desktop == L"192",
           "An app other than pdfcraft, notepad++ or acrobat is ignored, so that block has no app and is dropped." );

    struct bad_frame {
        const wchar_t *value;
        const char *what;
    };
    const bad_frame bad_frames[] = {
        { L"1 2 3", "A frame with three numbers" },
        { L"1 2 3 4 5", "A frame with five numbers" },
        { L"a b c d", "A frame of four words" },
        { L"1.5 2 3 4", "A frame with a number that is not whole" },
        { L"", "An empty frame" },
    };
    for ( const bad_frame &each : bad_frames ) {
        saved_session read = read_session( wstring( L"asked = no\r\n\r\n[window]\r\napp = pdfcraft\r\ndesktop = 150\r\nframe = " ) +
                                           each.value + L"\r\nfile = C:\\a.pdf\r\n" );
        check( read.windows.size() == 1 && zero_frame( read.windows[0].frame ) &&
                   read.windows[0].paths == vector<wstring>{ L"C:\\a.pdf" },
               ( std::string( each.what ) + " is ignored: the frame stays 0 0 0 0 and the window is still read." ).c_str() );
    }

    struct asked_value {
        const wchar_t *line;
        const char *what;
    };
    const asked_value not_yes[] = {
        { L"asked = no", "asked = no" },
        { L"asked = true", "asked = true" },
        { L"asked = 1", "asked = 1" },
        { L"asked =", "asked with no value" },
    };
    for ( const asked_value &each : not_yes )
        check( !read_session( wstring( each.line ) + L"\r\n" ).asked,
               ( std::string( each.what ) + " reads as not asked, since only yes means asked." ).c_str() );
    check( read_session( L"asked = yes\r\n" ).asked, "asked = yes reads as asked." );
    check( !read_session( L"\r\n[window]\r\napp = pdfcraft\r\n" ).asked, "A session with no asked line reads as not asked." );

    saved_session no_app = read_session( L"asked = no\r\n"
                                         L"\r\n"
                                         L"[window]\r\n"
                                         L"desktop = 150\r\n"
                                         L"frame = 0 0 10 10\r\n"
                                         L"file = C:\\a.pdf\r\n"
                                         L"\r\n"
                                         L"[window]\r\n"
                                         L"app = acrobat\r\n"
                                         L"file = C:\\b.pdf\r\n" );
    check( no_app.windows.size() == 1 && no_app.windows[0].app == L"acrobat" &&
               no_app.windows[0].paths == vector<wstring>{ L"C:\\b.pdf" },
           "A [window] block with no app line is dropped and the next block is still read." );
}

// ---- 2. Which windows count ----

void check_study_windows() {
    struct row {
        const wchar_t *exe_name;
        const wchar_t *class_name;
        bool study;
        const wchar_t *app;
    };
    const row rows[] = {
        { L"notepad++.exe", L"Notepad++", true, L"notepad++" },
        { L"NOTEPAD++.EXE", L"Notepad++", true, L"notepad++" },
        { L"notepad++.exe", L"#32770", false, L"notepad++" },
        { L"Acrobat.exe", L"AcrobatSDIWindow", true, L"acrobat" },
        { L"Acrobat.exe", L"AVL_AVPopup", false, L"acrobat" },
        { L"pdfcraft.exe", L"Window Class", true, L"pdfcraft" },
        { L"PdfCraft.exe", L"anything", true, L"pdfcraft" },
        { L"explorer.exe", L"CabinetWClass", false, L"" },
        { L"notepad.exe", L"Notepad", false, L"" },
    };
    for ( const row &each : rows ) {
        std::string name = to_utf8( each.exe_name ) + " with class " + to_utf8( each.class_name );
        check( is_study_window( each.exe_name, each.class_name ) == each.study,
               ( name + ( each.study ? " is a study window." : " is not a study window." ) ).c_str() );
        std::string app = to_utf8( each.app );
        check( app_of( each.exe_name ) == each.app,
               ( "app_of " + to_utf8( each.exe_name ) + " is " + ( app.empty() ? "empty" : "\"" + app + "\"" ) + "." ).c_str() );
    }
}

void check_pdfs_in_command_line() {
    check( pdfs_in_command_line( L"\"C:\\Program Files\\PdfCraft\\pdfcraft.exe\" --new-window --mode read \"C:\\a b\\x.pdf\" "
                                 L"\"C:\\y.PDF\"" ) == vector<wstring>{ L"C:\\a b\\x.pdf", L"C:\\y.PDF" },
           "A PdfCraft command line with two PDFs gives both, in order, without quotes, with .PDF matched in any case." );
    check( pdfs_in_command_line( L"\"C:\\...\\Acrobat.exe\" /n /A \"view=FitH\" \"C:\\s.pdf\"" ) == vector<wstring>{ L"C:\\s.pdf" },
           "An Acrobat command line gives only its PDF, never the options or their values." );
    check( pdfs_in_command_line( L"\"C:\\...\\pdfcraft.exe\" --control \"C:\\t\\ctl.txt\" \"C:\\z.pdf\"" ) ==
               vector<wstring>{ L"C:\\z.pdf" },
           "A PdfCraft command line with a control file gives only the PDF, not the control file." );
    check( pdfs_in_command_line( L"\"C:\\Program Files\\PdfCraft\\pdfcraft.exe\" --new-window --mode read" ).empty(),
           "A command line with no PDF gives an empty list." );
    check( pdfs_in_command_line( L"" ).empty(), "An empty command line gives an empty list." );
}

// ---- 3. Saving and closing ----

int notepad_tab_count( HWND window ) {
    return int( SendMessageW( window, WM_USER + 1000 + 7, 0, 1 ) );
}

bool notepad_reaches_tabs( HWND window, int count ) {
    for ( int wait = 0; wait < 30; ++wait ) {
        if ( window && notepad_tab_count( window ) == count ) return true;
        Sleep( 100 );
    }
    return false;
}

struct test_notepad {
    PROCESS_INFORMATION info{};
    HWND window = nullptr;
};

// A separate Notepad++, started minimized without taking the focus, and its main window found
// by its process id and class.
test_notepad start_test_notepad( const vector<wstring> &paths ) {
    test_notepad started;
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
        HWND each = nullptr;
        while ( ( each = FindWindowExW( nullptr, each, L"Notepad++", nullptr ) ) != nullptr ) {
            DWORD process_id = 0;
            GetWindowThreadProcessId( each, &process_id );
            if ( process_id == started.info.dwProcessId ) started.window = each;
        }
    }
    return started;
}

void end_test_notepad( test_notepad &started ) {
    if ( !started.info.hProcess ) return;
    if ( started.window && still_ours( started.window, started.info.dwProcessId ) )
        PostMessageW( started.window, WM_CLOSE, 0, 0 );
    if ( WaitForSingleObject( started.info.hProcess, 3000 ) != WAIT_OBJECT_0 ) TerminateProcess( started.info.hProcess, 0 );
    WaitForSingleObject( started.info.hProcess, 5000 );
    CloseHandle( started.info.hThread );
    CloseHandle( started.info.hProcess );
    started.info = {};
}

void check_saving_empty_lists() {
    vector<wstring> problems;
    vector<saved_window> recorded = save_and_close( {}, problems );
    check( recorded.empty(), "save_and_close of an empty list returns an empty list." );
    check( problems.empty(), "save_and_close of an empty list gives no problems." );

    HWND not_a_window = reinterpret_cast<HWND>( static_cast<uintptr_t>( 0x7FFFFFF0 ) );
    if ( IsWindow( not_a_window ) ) {
        check( true, "The made-up handle happens to be a window, so the check with a handle that is not a window is skipped." );
        return;
    }
    vector<wstring> handle_problems;
    vector<saved_window> handle_recorded = save_and_close( { not_a_window }, handle_problems );
    check( handle_recorded.empty(), "save_and_close skips a handle that is not a window and records nothing for it." );
    check( handle_problems.empty(), "save_and_close gives no problem for a handle that is not a window." );
}

void check_saving_notepad_with_files() {
    wstring folder = join_path( expand_variables( L"%TEMP%" ), L"crammy sessions check" );
    CreateDirectoryW( folder.c_str(), nullptr );
    wstring first = join_path( folder, L"first notes.txt" );
    wstring second = join_path( folder, L"second notes.txt" );
    wstring first_text = L"first scratch notes\r\nline two\r\n";
    wstring second_text = L"second scratch notes\r\n";
    write_text_file( first, first_text );
    write_text_file( second, second_text );

    test_notepad notepad = start_test_notepad( { first, second } );
    bool started = notepad.window && notepad_reaches_tabs( notepad.window, 2 );
    check( started, "A minimized scratch Notepad++ starts with the two scratch files open." );

    if ( started && still_ours( notepad.window, notepad.info.dwProcessId ) ) {
        wstring showing = current_desktop_name();
        vector<wstring> problems;
        vector<saved_window> recorded = save_and_close( { notepad.window }, problems );
        check( WaitForSingleObject( notepad.info.hProcess, 5000 ) == WAIT_OBJECT_0,
               "After save_and_close, the Notepad++ with two files has ended within 5 seconds." );
        check( problems.empty(), "save_and_close of a Notepad++ whose files are all saved gives no problems." );
        check( recorded.size() == 1, "save_and_close records the one Notepad++ window it was given." );
        if ( recorded.size() == 1 ) {
            check( recorded[0].app == L"notepad++", "The Notepad++ window is recorded with app notepad++." );
            check( recorded[0].paths == vector<wstring>{ first, second },
                   "Both scratch files are recorded, in tab order." );
            check( recorded[0].desktop == showing, "The Notepad++ window is recorded on the desktop it was on." );
        }
        check( read_text_file( first ) == first_text, "The first scratch file still holds what the test wrote." );
        check( read_text_file( second ) == second_text, "The second scratch file still holds what the test wrote." );
    }

    end_test_notepad( notepad );
    DeleteFileW( first.c_str() );
    DeleteFileW( second.c_str() );
    RemoveDirectoryW( folder.c_str() );
    check( !is_folder( folder ), "The scratch folder for the saving check is gone afterwards." );
}

void check_saving_empty_notepad() {
    test_notepad notepad = start_test_notepad( {} );
    bool started = notepad.window && notepad_reaches_tabs( notepad.window, 1 );
    check( started, "A minimized scratch Notepad++ starts with only its empty new 1 tab." );

    if ( started && still_ours( notepad.window, notepad.info.dwProcessId ) ) {
        vector<wstring> problems;
        vector<saved_window> recorded = save_and_close( { notepad.window }, problems );
        check( WaitForSingleObject( notepad.info.hProcess, 5000 ) == WAIT_OBJECT_0,
               "After save_and_close, the Notepad++ with only an empty new 1 tab has ended within 5 seconds." );
        check( problems.empty(), "An empty new 1 tab gives no problem; it is simply closed with the window." );
        check( recorded.size() == 1 && recorded[0].app == L"notepad++",
               "The Notepad++ with only an empty new 1 tab is recorded with app notepad++." );
        check( recorded.size() == 1 && recorded[0].paths.empty(),
               "The empty new 1 tab is not recorded, so the window has no paths." );
    }

    end_test_notepad( notepad );
}

// ---- 4. Reopening ----

planned_window frame_planned( const wstring &side, const wstring &desktop, bool has_frame, RECT frame ) {
    planned_window window;
    window.side = side;
    window.app = program::pdfcraft;
    window.paths = { L"C:\\notes.pdf" };
    window.desktop = desktop;
    window.has_frame = has_frame;
    window.frame = frame;
    return window;
}

void check_framed_targets() {
    RECT screen = { 0, 0, 1000, 600 };
    RECT frame = { -9, 0, 969, 1029 };

    vector<RECT> alone = window_targets( { frame_planned( L"left", L"150", true, frame ) }, screen );
    check( alone.size() == 1 && same_frame( alone[0], frame ), "A window with has_frame gets exactly its frame as its target." );

    vector<RECT> full = window_targets( { frame_planned( L"full", L"", true, RECT{ 100, 200, 300, 400 } ) }, screen );
    check( full.size() == 1 && same_frame( full[0], RECT{ 100, 200, 300, 400 } ),
           "A full window with has_frame gets its frame, not the whole screen." );

    vector<RECT> mixed = window_targets( { frame_planned( L"left", L"150", false, {} ),
                                           frame_planned( L"left", L"150", true, frame ),
                                           frame_planned( L"left", L"150", false, {} ) },
                                         screen );
    check( mixed.size() == 3, "Three planned windows give three targets." );
    if ( mixed.size() == 3 ) {
        check( same_frame( mixed[0], RECT{ 0, 0, 500, 300 } ) && same_frame( mixed[2], RECT{ 0, 300, 500, 600 } ),
               "Two left windows on desktop 150 still split the left half into two rows when a third left window "
               "on that desktop has a frame." );
        check( same_frame( mixed[1], frame ), "The left window with a frame among them gets exactly its frame." );
    }
}

struct reopen_files {
    wstring folder, first_pdf, second_pdf, notes, missing_pdf, missing_notes;
};

reopen_files make_reopen_files() {
    reopen_files files;
    files.folder = join_path( expand_variables( L"%TEMP%" ), L"crammy reopen check" );
    CreateDirectoryW( files.folder.c_str(), nullptr );
    files.first_pdf = join_path( files.folder, L"first.pdf" );
    files.second_pdf = join_path( files.folder, L"second slides.pdf" );
    files.notes = join_path( files.folder, L"notes.txt" );
    files.missing_pdf = join_path( files.folder, L"gone.pdf" );
    files.missing_notes = join_path( files.folder, L"gone notes.txt" );
    write_text_file( files.first_pdf, L"%PDF-1.4\r\n" );
    write_text_file( files.second_pdf, L"%PDF-1.4\r\n" );
    write_text_file( files.notes, L"notes\r\n" );
    DeleteFileW( files.missing_pdf.c_str() );
    DeleteFileW( files.missing_notes.c_str() );
    return files;
}

void remove_reopen_files( const reopen_files &files ) {
    DeleteFileW( files.first_pdf.c_str() );
    DeleteFileW( files.second_pdf.c_str() );
    DeleteFileW( files.notes.c_str() );
    RemoveDirectoryW( files.folder.c_str() );
}

void check_reopen_plan() {
    reopen_files files = make_reopen_files();
    check( is_file( files.first_pdf ) && is_file( files.second_pdf ) && is_file( files.notes ) && !is_file( files.missing_pdf ),
           "The scratch files for the reopen checks are made, and the missing ones do not exist." );

    check( reopen_plan( saved_session{} ).empty(), "reopen_plan of an empty session gives no windows." );

    RECT pdf_frame = { 0, 0, 1152, 1020 };
    saved_session one;
    one.windows.push_back( saved( L"pdfcraft", L"150", pdf_frame, { files.first_pdf, files.missing_pdf, files.second_pdf } ) );
    vector<planned_window> one_plan = reopen_plan( one );
    check( one_plan.size() == 1, "A saved PdfCraft window with files gives one planned window." );
    if ( one_plan.size() == 1 ) {
        check( one_plan[0].app == program::pdfcraft, "A saved pdfcraft window is planned with the app pdfcraft." );
        check( one_plan[0].paths == vector<wstring>{ files.first_pdf, files.second_pdf },
               "A file that no longer exists is left out, and the other files keep their order." );
        check( one_plan[0].desktop == L"150", "The saved desktop is copied to the planned window." );
        check( one_plan[0].has_frame, "The planned window has has_frame set." );
        check( same_frame( one_plan[0].frame, pdf_frame ), "The saved frame is copied to the planned window." );
        check( one_plan[0].side == L"full", "The planned window's side is full." );
    }

    saved_session only_missing;
    only_missing.windows.push_back( saved( L"notepad++", L"150", pdf_frame, { files.missing_notes } ) );
    only_missing.windows.push_back( saved( L"pdfcraft", L"192", pdf_frame, { files.missing_pdf } ) );
    check( reopen_plan( only_missing ).empty(), "A window left with no files, since all its files are missing, is left out." );

    saved_session no_files;
    no_files.windows.push_back( saved( L"pdfcraft", L"150", pdf_frame, {} ) );
    check( reopen_plan( no_files ).empty(), "A saved window with no files at all is left out." );

    RECT acrobat_frame = { -9, 0, 969, 1029 };
    saved_session acrobat;
    acrobat.windows.push_back( saved( L"acrobat", L"Main", acrobat_frame, { files.first_pdf, files.second_pdf } ) );
    vector<planned_window> acrobat_plan = reopen_plan( acrobat );
    check( acrobat_plan.size() == 2, "A saved Acrobat window with two files gives two planned windows." );
    if ( acrobat_plan.size() == 2 ) {
        check( acrobat_plan[0].app == program::acrobat && acrobat_plan[1].app == program::acrobat,
               "Both planned windows from a saved Acrobat window have the app acrobat." );
        check( acrobat_plan[0].paths == vector<wstring>{ files.first_pdf } &&
                   acrobat_plan[1].paths == vector<wstring>{ files.second_pdf },
               "Each planned Acrobat window has one file, in the saved order." );
        check( acrobat_plan[0].desktop == L"Main" && acrobat_plan[1].desktop == L"Main",
               "Each planned Acrobat window gets the saved desktop." );
        check( acrobat_plan[0].has_frame && acrobat_plan[1].has_frame && same_frame( acrobat_plan[0].frame, acrobat_frame ) &&
                   same_frame( acrobat_plan[1].frame, acrobat_frame ),
               "Each planned Acrobat window has has_frame set and the saved frame." );
    }

    RECT notes_frame = { 960, 0, 1920, 1040 };
    saved_session notes;
    notes.windows.push_back( saved( L"notepad++", L"", notes_frame, { files.notes } ) );
    vector<planned_window> notes_plan = reopen_plan( notes );
    check( notes_plan.size() == 1 && notes_plan[0].app == program::notepad_pp &&
               notes_plan[0].paths == vector<wstring>{ files.notes },
           "A saved notepad++ window is planned with the app notepad_pp and its file." );
    check( notes_plan.size() == 1 && notes_plan[0].desktop.empty() && notes_plan[0].has_frame &&
               same_frame( notes_plan[0].frame, notes_frame ),
           "A saved window with no desktop keeps the empty desktop and its frame." );

    saved_session mixed;
    mixed.windows.push_back( saved( L"pdfcraft", L"150", pdf_frame, { files.first_pdf, files.missing_pdf } ) );
    mixed.windows.push_back( saved( L"notepad++", L"150", notes_frame, { files.missing_notes } ) );
    mixed.windows.push_back( saved( L"acrobat", L"192", acrobat_frame, { files.first_pdf, files.second_pdf } ) );
    mixed.windows.push_back( saved( L"notepad++", L"115", notes_frame, { files.notes } ) );
    vector<planned_window> mixed_plan = reopen_plan( mixed );
    check( mixed_plan.size() == 4 && mixed_plan[0].app == program::pdfcraft && mixed_plan[1].app == program::acrobat &&
               mixed_plan[2].app == program::acrobat && mixed_plan[3].app == program::notepad_pp,
           "A session of several windows gives its planned windows in the saved order, without the window whose files are missing." );
    check( mixed_plan.size() == 4 && mixed_plan[0].desktop == L"150" && mixed_plan[1].desktop == L"192" &&
               mixed_plan[2].desktop == L"192" && mixed_plan[3].desktop == L"115",
           "Each planned window in a session of several windows gets the desktop of its own saved window." );

    remove_reopen_files( files );
    check( !is_folder( files.folder ), "The scratch folder for the reopen checks is gone afterwards." );
}

// ---- 5. The command line ----

void check_session_actions() {
    command_line save = parse_command_line( { L"--save-and-close" } );
    check( save.action == L"save-and-close" && save.error.empty(), "--save-and-close sets the action to save-and-close." );
    check( parse_command_line( { L"--SAVE-AND-CLOSE" } ).action == L"save-and-close",
           "--SAVE-AND-CLOSE in capitals sets the action to save-and-close." );
    command_line reopen = parse_command_line( { L"--reopen" } );
    check( reopen.action == L"reopen" && reopen.error.empty(), "--reopen sets the action to reopen." );
    check( parse_command_line( { L"--Reopen" } ).action == L"reopen", "--Reopen in mixed case sets the action to reopen." );
}

} // namespace

void test_sessions() {
    check_writing_session();
    check_round_trips();
    check_reading_session();
    check_study_windows();
    check_pdfs_in_command_line();
    check_saving_empty_lists();
    check_saving_notepad_with_files();
    check_saving_empty_notepad();
    check_framed_targets();
    check_reopen_plan();
    check_session_actions();
}
