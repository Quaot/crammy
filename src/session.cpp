#include "session.h"

#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>

#include "desktops.h"
#include "profiles.h"

namespace {

// Commands Notepad++ accepts from other programs (from its Notepad_plus_msgs.h)
const UINT npp_message = WM_USER + 1000;
const UINT npp_open_file_count = npp_message + 7;
const UINT npp_current_doc_index = npp_message + 23;   // -1 for a view that is not showing
const UINT npp_switch_to_tab = npp_message + 28;
const UINT npp_save_tab = npp_message + 38;
const WPARAM main_view = 0, second_view = 1;

// Scintilla, the editor inside each Notepad++ view. These take no pointers, so they work from here.
const UINT sci_get_length = 2006;
const UINT sci_set_save_point = 2014;

wstring frame_text( const RECT &frame ) {
    return std::to_wstring( frame.left ) + L" " + std::to_wstring( frame.top ) + L" " +
           std::to_wstring( frame.right ) + L" " + std::to_wstring( frame.bottom );
}

bool read_number( const wstring &text, long &number ) {
    if ( text.empty() ) return false;
    size_t start = ( text[0] == L'-' ) ? 1 : 0;
    if ( start == text.size() ) return false;
    for ( size_t i = start; i < text.size(); ++i ) if ( text[i] < L'0' || text[i] > L'9' ) return false;
    number = std::stol( text );
    return true;
}

bool read_frame( const wstring &text, RECT &frame ) {
    vector<wstring> words;
    size_t start = 0;
    while ( start < text.size() ) {
        size_t end = text.find( L' ', start );
        if ( end == wstring::npos ) end = text.size();
        if ( end > start ) words.push_back( text.substr( start, end - start ) );
        start = end + 1;
    }
    long numbers[4];
    if ( words.size() != 4 ) return false;
    for ( int i = 0; i < 4; ++i ) if ( !read_number( words[i], numbers[i] ) ) return false;
    frame = { numbers[0], numbers[1], numbers[2], numbers[3] };
    return true;
}

wstring program_of( HWND window, DWORD *process_id = nullptr ) {
    DWORD id = 0;
    GetWindowThreadProcessId( window, &id );
    if ( process_id ) *process_id = id;
    HANDLE process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, id );
    if ( !process ) return L"";
    wchar_t path[MAX_PATH * 4];
    DWORD size = MAX_PATH * 4;
    wstring result = QueryFullProcessImageNameW( process, 0, path, &size ) ? path : L"";
    CloseHandle( process );
    return result;
}

// Another program's command line, read through ntdll's ProcessCommandLineInformation (class 60)
wstring command_line_of( DWORD process_id ) {
    struct unicode_text { USHORT length, maximum; PWSTR buffer; };
    using query_function = LONG( WINAPI * )( HANDLE, ULONG, PVOID, ULONG, PULONG );
    static query_function query = reinterpret_cast<query_function>(
        reinterpret_cast<void *>( GetProcAddress( GetModuleHandleW( L"ntdll.dll" ), "NtQueryInformationProcess" ) ) );
    if ( !query ) return L"";
    HANDLE process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id );
    if ( !process ) return L"";
    vector<BYTE> buffer( 64 * 1024 );
    ULONG size = 0;
    wstring result;
    if ( query( process, 60, buffer.data(), static_cast<ULONG>( buffer.size() ), &size ) >= 0 ) {
        const unicode_text *text = reinterpret_cast<const unicode_text *>( buffer.data() );
        result.assign( text->buffer, text->length / sizeof( wchar_t ) );
    }
    CloseHandle( process );
    return result;
}

wstring class_of( HWND window ) {
    wchar_t name[64] = {};
    GetClassNameW( window, name, 64 );
    return name;
}

wstring title_text( HWND window ) {
    wchar_t title[1024] = {};
    GetWindowTextW( window, title, 1024 );
    return title;
}

// The window's frame as it is on the screen; for a minimized window, where it goes when restored
RECT frame_of( HWND window ) {
    RECT frame{};
    if ( IsIconic( window ) ) {
        WINDOWPLACEMENT placement{ sizeof( placement ) };
        GetWindowPlacement( window, &placement );
        return placement.rcNormalPosition;
    }
    if ( FAILED( DwmGetWindowAttribute( window, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof( frame ) ) ) )
        GetWindowRect( window, &frame );
    return frame;
}

wstring desktop_name_of( HWND window ) {
    vector<wstring> names = desktop_names();
    for ( const window_on_desktop &each : windows_by_desktop() ) {
        if ( each.window == window && each.desktop_index >= 0 && each.desktop_index < static_cast<int>( names.size() ) )
            return names[each.desktop_index];
    }
    return L"";
}

// Notepad++ titles itself "*C:\folder\file.txt - Notepad++", with * while there are unsaved changes
wstring shown_in_notepad( HWND window ) {
    wstring title = title_text( window );
    if ( starts_with( title, L"*" ) ) title.erase( 0, 1 );
    size_t end = title.rfind( L" - Notepad++" );
    return end == wstring::npos ? title : title.substr( 0, end );
}

// The editor of a view: the first Scintilla child for the main view, the second for the other
HWND editor_of( HWND window, WPARAM view ) {
    HWND editor = FindWindowExW( window, nullptr, L"Scintilla", nullptr );
    if ( view == second_view && editor ) editor = FindWindowExW( window, editor, L"Scintilla", nullptr );
    return editor;
}

// Saves every file in a Notepad++ window and lists them. False when the window must stay open.
bool save_notepad( HWND window, vector<wstring> &paths, vector<wstring> &problems ) {
    bool can_close = true;
    for ( WPARAM view : { main_view, second_view } ) {
        if ( view == second_view && int( SendMessageW( window, npp_current_doc_index, 0, second_view ) ) < 0 ) continue;
        int count = int( SendMessageW( window, npp_open_file_count, 0, view == main_view ? 1 : 2 ) );
        for ( int tab = 0; tab < count; ++tab ) {
            SendMessageW( window, npp_switch_to_tab, view, tab );
            wstring path = shown_in_notepad( window );
            HWND editor = editor_of( window, view );
            if ( !is_file( path ) ) {
                if ( editor && SendMessageW( editor, sci_get_length, 0, 0 ) == 0 ) {
                    SendMessageW( editor, sci_set_save_point, 0, 0 );   // empty, so nothing to ask about
                } else {
                    problems.push_back( L"Notepad++ has an unsaved new tab with text in it. Save it yourself, then try again." );
                    can_close = false;
                }
                continue;
            }
            SendMessageW( window, npp_save_tab, 0, 0 );
            if ( starts_with( title_text( window ), L"*" ) ) {
                problems.push_back( L"Notepad++ could not save " + file_name( path ) + L", so its window stays open." );
                can_close = false;
            }
            paths.push_back( path );
        }
    }
    return can_close;
}

}  // namespace

wstring write_session( const saved_session &session ) {
    wstring text = wstring( L"asked = " ) + ( session.asked ? L"yes" : L"no" ) + L"\r\n";
    for ( const saved_window &window : session.windows ) {
        text += L"\r\n[window]\r\n";
        text += L"app = " + window.app + L"\r\n";
        text += L"desktop = " + window.desktop + L"\r\n";
        text += L"frame = " + frame_text( window.frame ) + L"\r\n";
        for ( const wstring &path : window.paths ) text += L"file = " + path + L"\r\n";
    }
    // "desktop = " with nothing after it ends in a space; keep the file free of trailing spaces
    wstring cleaned;
    size_t start = 0;
    while ( start < text.size() ) {
        size_t end = text.find( L"\r\n", start );
        wstring line = text.substr( start, end - start );
        while ( !line.empty() && line.back() == L' ' ) line.pop_back();
        cleaned += line + L"\r\n";
        start = end + 2;
    }
    return cleaned;
}

saved_session read_session( const wstring &text ) {
    saved_session session;
    vector<saved_window> windows;
    bool in_window = false;
    size_t start = 0;
    while ( start < text.size() ) {
        size_t end = text.find( L'\n', start );
        if ( end == wstring::npos ) end = text.size();
        wstring line = trim( text.substr( start, end - start ) );
        start = end + 1;
        if ( line.empty() || line[0] == L'#' ) continue;
        if ( lower( line ) == L"[window]" ) {
            windows.push_back( {} );
            in_window = true;
            continue;
        }
        size_t equals = line.find( L'=' );
        if ( equals == wstring::npos ) continue;
        wstring name = lower( trim( line.substr( 0, equals ) ) );
        wstring value = trim( line.substr( equals + 1 ) );
        if ( !in_window ) {
            if ( name == L"asked" ) session.asked = lower( value ) == L"yes";
            continue;
        }
        saved_window &window = windows.back();
        if ( name == L"app" ) {
            wstring app = lower( value );
            if ( app == L"pdfcraft" || app == L"notepad++" || app == L"acrobat" ) window.app = app;
        } else if ( name == L"desktop" ) {
            window.desktop = value;
        } else if ( name == L"frame" ) {
            RECT frame;
            if ( read_frame( value, frame ) ) window.frame = frame;
        } else if ( name == L"file" ) {
            if ( !value.empty() ) window.paths.push_back( value );
        }
    }
    for ( const saved_window &window : windows ) if ( !window.app.empty() ) session.windows.push_back( window );
    return session;
}

bool is_study_window( const wstring &exe_name, const wstring &class_name ) {
    wstring app = app_of( exe_name );
    if ( app == L"pdfcraft" ) return true;
    if ( app == L"notepad++" ) return class_name == L"Notepad++";
    if ( app == L"acrobat" ) return class_name == L"AcrobatSDIWindow";
    return false;
}

wstring app_of( const wstring &exe_name ) {
    wstring name = lower( file_name( exe_name ) );
    if ( name == L"pdfcraft.exe" ) return L"pdfcraft";
    if ( name == L"notepad++.exe" ) return L"notepad++";
    if ( name == L"acrobat.exe" ) return L"acrobat";
    return L"";
}

vector<wstring> pdfs_in_command_line( const wstring &command_line ) {
    vector<wstring> pdfs;
    if ( trim( command_line ).empty() ) return pdfs;
    int count = 0;
    wchar_t **arguments = CommandLineToArgvW( command_line.c_str(), &count );
    if ( !arguments ) return pdfs;
    for ( int i = 1; i < count; ++i ) {
        wstring argument = arguments[i];
        if ( ends_with( lower( argument ), L".pdf" ) ) pdfs.push_back( argument );
    }
    LocalFree( arguments );
    return pdfs;
}

vector<saved_window> save_and_close( const vector<HWND> &windows, vector<wstring> &problems ) {
    vector<saved_window> saved;
    vector<HWND> closing;
    for ( HWND window : windows ) {
        if ( !window || !IsWindow( window ) ) continue;
        DWORD process_id = 0;
        saved_window record;
        record.app = app_of( program_of( window, &process_id ) );
        record.desktop = desktop_name_of( window );
        record.frame = frame_of( window );

        bool can_close = true;
        if ( record.app == L"notepad++" ) can_close = save_notepad( window, record.paths, problems );
        else record.paths = pdfs_in_command_line( command_line_of( process_id ) );

        saved.push_back( record );
        if ( can_close ) {
            PostMessageW( window, WM_CLOSE, 0, 0 );
            closing.push_back( window );
        }
    }

    // A program asking about unsaved changes keeps its window; say which one
    DWORD started = GetTickCount();
    for ( HWND window : closing ) {
        while ( IsWindow( window ) && IsWindowVisible( window ) && GetTickCount() - started < 5000 ) Sleep( 100 );
        if ( IsWindow( window ) && IsWindowVisible( window ) ) {
            wstring app = app_of( program_of( window ) );
            problems.push_back( ( app == L"pdfcraft" ? L"PdfCraft" : app == L"acrobat" ? L"Acrobat" : L"Notepad++" ) +
                                wstring( L" did not close '" ) + title_text( window ) + L"'. It may be asking about unsaved changes." );
        }
    }
    return saved;
}

int save_and_close_all( const wstring &path, vector<wstring> &problems ) {
    vector<HWND> windows;
    EnumWindows( []( HWND window, LPARAM list ) -> BOOL {
        if ( IsWindowVisible( window ) && !GetWindow( window, GW_OWNER ) && GetWindowTextLengthW( window ) > 0 &&
             is_study_window( program_of( window ), class_of( window ) ) )
            reinterpret_cast<vector<HWND> *>( list )->push_back( window );
        return TRUE;
    }, reinterpret_cast<LPARAM>( &windows ) );

    saved_session session;
    session.windows = save_and_close( windows, problems );
    if ( !write_text_file( path, write_session( session ) ) ) problems.push_back( L"Could not write " + path + L"." );
    return static_cast<int>( session.windows.size() );
}

wstring session_path() {
    return join_path( parent_folder( profiles_path() ), L"last session.txt" );
}

vector<planned_window> reopen_plan( const saved_session &session ) {
    vector<planned_window> plan;
    for ( const saved_window &window : session.windows ) {
        vector<wstring> files;
        for ( const wstring &path : window.paths ) if ( is_file( path ) ) files.push_back( path );
        if ( files.empty() ) continue;

        planned_window planned;
        planned.side = L"full";
        planned.app = window.app == L"notepad++" ? program::notepad_pp
                    : window.app == L"acrobat" ? program::acrobat : program::pdfcraft;
        planned.desktop = window.desktop;
        planned.has_frame = true;
        planned.frame = window.frame;
        if ( planned.app == program::acrobat ) {
            for ( const wstring &file : files ) {
                planned.paths = { file };
                plan.push_back( planned );
            }
        } else {
            planned.paths = files;
            plan.push_back( planned );
        }
    }
    return plan;
}

vector<wstring> reopen_session( const wstring &path ) {
    vector<wstring> problems;
    saved_session session = read_session( read_text_file( path ) );
    vector<planned_window> plan = reopen_plan( session );

    // A window for the desktop showing opens as it is; one for a desktop that is gone opens here too
    vector<wstring> names = desktop_names();
    wstring showing = lower( current_desktop_name() );
    for ( planned_window &window : plan ) {
        bool exists = false;
        for ( const wstring &name : names ) if ( lower( name ) == lower( window.desktop ) ) exists = true;
        if ( !window.desktop.empty() && !exists ) problems.push_back( L"There is no desktop named '" + window.desktop + L"' any more, so its windows opened on this one." );
        if ( !exists || lower( window.desktop ) == showing ) window.desktop.clear();
    }
    if ( !plan.empty() ) open_windows_recorded( plan, problems );

    session.asked = true;
    write_text_file( path, write_session( session ) );
    return problems;
}
