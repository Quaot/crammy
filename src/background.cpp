#include "background.h"

#include <shlobj.h>
#include <objbase.h>
#include <shellapi.h>
#include <taskschd.h>
#include <oleauto.h>

#include <cstdio>
#include <exception>

#include "next_runner.h"
#include "panel.h"
#include "session.h"
#include "settings.h"
#include "tiling.h"
#include "timetable.h"
#include "ui.h"

const wchar_t *background_class_name = L"CrammyBackground";

namespace {

const UINT tray_message = WM_APP + 20;
const int hotkey_id = 1;
const UINT tray_id = 1;
const UINT menu_setups = 1;
const UINT menu_timetable = 2;
const UINT menu_start_at_sign_in = 3;
const UINT menu_quit = 4;
const UINT menu_save_and_close = 5;
const UINT menu_reopen = 6;

UINT taskbar_created_message = 0;

void log_now( const wstring &text ) {
    SYSTEMTIME now;
    GetLocalTime( &now );
    append_log( background_log_path(), log_line( now, text ), 200 );
}

// A crash leaves no report from Windows for this program, so it is written down here
LONG WINAPI log_crash( EXCEPTION_POINTERS *info ) {
    auto address = reinterpret_cast<unsigned long long>( info->ExceptionRecord->ExceptionAddress );
    auto base = reinterpret_cast<unsigned long long>( GetModuleHandleW( nullptr ) );
    log_now( crash_text( info->ExceptionRecord->ExceptionCode, address >= base ? address - base : address ) );
    return EXCEPTION_CONTINUE_SEARCH;
}

void log_uncaught() {
    wstring what = L"unknown";
    try {
        if ( std::exception_ptr error = std::current_exception() ) std::rethrow_exception( error );
    } catch ( const std::exception &error ) {
        what = from_utf8( error.what() );
    } catch ( ... ) {
    }
    log_now( L"ended by an uncaught C++ exception: " + what );
    std::abort();
}

HWND find_background_window() {
    return FindWindowW( background_class_name, nullptr );
}

wstring program_path() {
    wchar_t path[MAX_PATH * 4];
    DWORD length = GetModuleFileNameW( nullptr, path, MAX_PATH * 4 );
    return wstring( path, length );
}

bool shortcut_exists() {
    return is_file( startup_shortcut_path() );
}

wstring escape_xml( const wstring &text ) {
    wstring escaped;
    for ( wchar_t c : text ) {
        if ( c == L'&' ) escaped += L"&amp;";
        else if ( c == L'<' ) escaped += L"&lt;";
        else if ( c == L'>' ) escaped += L"&gt;";
        else escaped += c;
    }
    return escaped;
}

// Calls use with the root folder of Task Scheduler; false when Task Scheduler cannot be reached.
template <typename use_folder>
bool with_task_folder( use_folder use ) {
    HRESULT started = CoInitialize( nullptr );
    bool done = false;
    ITaskService *service = nullptr;
    if ( SUCCEEDED( CoCreateInstance( CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &service ) ) ) ) {
        VARIANT empty;
        VariantInit( &empty );
        ITaskFolder *folder = nullptr;
        BSTR root = SysAllocString( L"\\" );
        if ( SUCCEEDED( service->Connect( empty, empty, empty, empty ) ) && SUCCEEDED( service->GetFolder( root, &folder ) ) ) {
            done = use( folder );
            folder->Release();
        }
        SysFreeString( root );
        service->Release();
    }
    if ( SUCCEEDED( started ) ) CoUninitialize();
    return done;
}

bool register_sign_in_task() {
    wstring exe = join_path( parent_folder( program_path() ), L"crammy.exe" );
    wstring xml = sign_in_task_xml( exe, current_account() );
    return with_task_folder( [&]( ITaskFolder *folder ) {
        BSTR name = SysAllocString( sign_in_task_name().c_str() );
        BSTR text = SysAllocString( xml.c_str() );
        VARIANT empty;
        VariantInit( &empty );
        IRegisteredTask *task = nullptr;
        HRESULT result = folder->RegisterTask( name, text, TASK_CREATE_OR_UPDATE, empty, empty, TASK_LOGON_INTERACTIVE_TOKEN, empty, &task );
        if ( task ) task->Release();
        SysFreeString( text );
        SysFreeString( name );
        return SUCCEEDED( result );
    } );
}

bool remove_sign_in_task() {
    return with_task_folder( []( ITaskFolder *folder ) {
        BSTR name = SysAllocString( sign_in_task_name().c_str() );
        HRESULT result = folder->DeleteTask( name, 0 );
        SysFreeString( name );
        return SUCCEEDED( result ) || result == HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND );
    } );
}

bool make_shortcut( const wstring &path ) {
    HRESULT started = CoInitialize( nullptr );
    bool made = false;
    IShellLinkW *link = nullptr;
    if ( SUCCEEDED( CoCreateInstance( CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &link ) ) ) ) {
        wstring exe = join_path( parent_folder( program_path() ), L"crammy.exe" );
        link->SetPath( exe.c_str() );
        link->SetArguments( L"--background" );
        link->SetWorkingDirectory( parent_folder( exe ).c_str() );
        IPersistFile *file = nullptr;
        if ( SUCCEEDED( link->QueryInterface( IID_PPV_ARGS( &file ) ) ) ) {
            made = SUCCEEDED( file->Save( path.c_str(), TRUE ) );
            file->Release();
        }
        link->Release();
    }
    if ( SUCCEEDED( started ) ) CoUninitialize();
    return made;
}

crammy_settings current_settings() {
    vector<wstring> errors;
    return read_settings( read_text_file( settings_path() ), errors );
}

void flip_start_at_sign_in() {
    vector<wstring> errors;
    crammy_settings settings = read_settings( read_text_file( settings_path() ), errors );
    settings.start_at_sign_in = !start_at_sign_in_is_on();
    // Rewriting a file with mistakes in it would replace the lines that could not be read.
    if ( errors.empty() ) write_text_file( settings_path(), write_settings( settings ) );
    set_start_at_sign_in( settings.start_at_sign_in );
}

void shortcut_keys( UINT &modifiers, UINT &key ) {
    crammy_settings settings = current_settings();
    modifiers = settings.shortcut_modifiers;
    key = settings.shortcut_key;
}

NOTIFYICONDATAW tray_data( HWND window ) {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof( data );
    data.hWnd = window;
    data.uID = tray_id;
    return data;
}

void add_tray_icon( HWND window ) {
    NOTIFYICONDATAW data = tray_data( window );
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = tray_message;
    data.hIcon = LoadIconW( GetModuleHandleW( nullptr ), MAKEINTRESOURCEW( 1 ) );
    lstrcpyW( data.szTip, L"Crammy" );
    Shell_NotifyIconW( NIM_ADD, &data );
}

void remove_tray_icon( HWND window ) {
    NOTIFYICONDATAW data = tray_data( window );
    Shell_NotifyIconW( NIM_DELETE, &data );
}

void edit_timetable() {
    start_process( quoted( program_file( program::notepad_pp ) ) + L" " + quoted( timetable_path() ) );
}

// The saved session, when it records at least one window
bool has_saved_session( saved_session &session ) {
    if ( !is_file( session_path() ) ) return false;
    session = read_session( read_text_file( session_path() ) );
    return !session.windows.empty();
}

void save_and_close_everything() {
    vector<wstring> problems;
    int count = save_and_close_all( session_path(), problems );
    wstring text = L"Saved " + std::to_wstring( count ) + ( count == 1 ? L" window." : L" windows." );
    for ( const wstring &problem : problems ) text += L"\r\n\r\n" + problem;
    MessageBoxW( nullptr, text.c_str(), L"Crammy", MB_OK | ( problems.empty() ? MB_ICONINFORMATION : MB_ICONWARNING ) );
}

void reopen_last_session() {
    vector<wstring> problems = reopen_session( session_path() );
    wstring text;
    for ( const wstring &problem : problems ) text += ( text.empty() ? L"" : L"\r\n\r\n" ) + problem;
    if ( !text.empty() ) MessageBoxW( nullptr, text.c_str(), L"Crammy", MB_OK | MB_ICONWARNING );
}

// Asks once, after sign-in, whether to put back what was open before the PC was shut down
void ask_about_saved_session() {
    saved_session session;
    if ( !has_saved_session( session ) || session.asked ) return;
    vector<wstring> desktops;
    for ( const saved_window &window : session.windows ) {
        bool listed = window.desktop.empty();
        for ( const wstring &each : desktops ) if ( lower( each ) == lower( window.desktop ) ) listed = true;
        if ( !listed ) desktops.push_back( window.desktop );
    }
    size_t count = session.windows.size();
    wstring text = L"Reopen what you had open last time? " + std::to_wstring( count ) + ( count == 1 ? L" window" : L" windows" );
    for ( size_t i = 0; i < desktops.size(); ++i )
        text += ( i == 0 ? L" on desktops " : i + 1 == desktops.size() ? L" and " : L", " ) + desktops[i];
    text += L".";
    if ( MessageBoxW( nullptr, text.c_str(), L"Crammy", MB_YESNO | MB_ICONQUESTION ) == IDYES ) {
        reopen_last_session();   // marks it asked
        return;
    }
    session.asked = true;
    write_text_file( session_path(), write_session( session ) );
}

void show_tray_menu( HWND window ) {
    saved_session session;
    HMENU menu = CreatePopupMenu();
    AppendMenuW( menu, MF_STRING, menu_setups, L"Setups\x2026" );
    AppendMenuW( menu, MF_STRING, menu_timetable, L"Edit timetable" );
    AppendMenuW( menu, MF_STRING | ( start_at_sign_in_is_on() ? MF_CHECKED : 0 ), menu_start_at_sign_in, L"Start at sign-in" );
    AppendMenuW( menu, MF_STRING, menu_save_and_close, L"Save and close all" );
    AppendMenuW( menu, MF_STRING | ( has_saved_session( session ) ? 0 : MF_GRAYED ), menu_reopen, L"Reopen last session" );
    AppendMenuW( menu, MF_SEPARATOR, 0, nullptr );
    AppendMenuW( menu, MF_STRING, menu_quit, L"Quit" );

    POINT cursor;
    GetCursorPos( &cursor );
    SetForegroundWindow( window );   // without this the menu stays open after a click elsewhere
    UINT chosen = TrackPopupMenu( menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, window, nullptr );
    DestroyMenu( menu );

    if ( chosen == menu_setups ) show_setups();
    else if ( chosen == menu_timetable ) edit_timetable();
    else if ( chosen == menu_start_at_sign_in ) flip_start_at_sign_in();
    else if ( chosen == menu_save_and_close ) save_and_close_everything();
    else if ( chosen == menu_reopen ) reopen_last_session();
    else if ( chosen == menu_quit ) {
        log_now( L"stopped: Quit was chosen" );
        DestroyWindow( window );
    }
}

LRESULT CALLBACK background_proc( HWND window, UINT message, WPARAM wparam, LPARAM lparam ) {
    if ( message == show_panel_message() ) {
        // wparam 1 asks only to show the panel; 0 shows or hides it
        if ( wparam == 1 ) show_panel();
        else toggle_panel();
        return 0;
    }
    if ( message == next_message() ) {
        run_next();
        return 0;
    }
    if ( message == taskbar_created_message ) {
        add_tray_icon( window );
        return 0;
    }
    switch ( message ) {
    case tray_message:
        if ( LOWORD( lparam ) == WM_LBUTTONUP ) toggle_panel();
        else if ( LOWORD( lparam ) == WM_RBUTTONUP ) show_tray_menu( window );
        return 0;
    case WM_TIMER:
        on_next_timer( wparam );
        return 0;
    case WM_HOTKEY:
        if ( wparam == hotkey_id ) toggle_panel();
        return 0;
    case WM_CLOSE:
        // Quit destroys the window itself, so a request to close can only come from another program
        log_now( L"ignored a request to close" );
        return 0;
    case WM_ENDSESSION:
        if ( wparam ) log_now( L"Windows is ending the session" );
        return 0;
    case WM_DESTROY:
        log_now( L"stopped" );
        UnregisterHotKey( window, hotkey_id );
        remove_tray_icon( window );
        PostQuitMessage( 0 );
        return 0;
    }
    return DefWindowProcW( window, message, wparam, lparam );
}

}  // namespace

LRESULT CALLBACK background_window_proc( HWND window, UINT message, WPARAM wparam, LPARAM lparam ) {
    return background_proc( window, message, wparam, lparam );
}

wstring background_log_path() {
    return join_path( parent_folder( program_path() ), L"background log.txt" );
}

wstring log_line( const SYSTEMTIME &at, const wstring &text ) {
    wchar_t stamp[32];
    swprintf( stamp, 32, L"%04u-%02u-%02u %02u:%02u:%02u  ", at.wYear, at.wMonth, at.wDay, at.wHour, at.wMinute, at.wSecond );
    return stamp + text;
}

wstring crash_text( DWORD code, unsigned long long offset ) {
    wchar_t text[96];
    swprintf( text, 96, L"crashed with exception 0x%08lX at crammy.exe+0x%llX", static_cast<unsigned long>( code ), offset );
    return text;
}

void append_log( const wstring &path, const wstring &line, size_t keep ) {
    vector<wstring> lines;
    wstring old = read_text_file( path );
    size_t start = 0;
    while ( start < old.size() ) {
        size_t end = old.find( L'\n', start );
        if ( end == wstring::npos ) end = old.size();
        wstring each = old.substr( start, end - start );
        if ( !each.empty() && each.back() == L'\r' ) each.pop_back();
        lines.push_back( each );
        start = end + 1;
    }
    lines.push_back( line );
    size_t first = lines.size() > keep ? lines.size() - keep : 0;
    wstring text;
    for ( size_t i = first; i < lines.size(); ++i ) text += lines[i] + L"\r\n";
    write_text_file( path, text );
}

UINT show_panel_message() {
    static UINT message = RegisterWindowMessageW( L"CrammyShowPanel" );
    return message;
}

bool background_is_running() {
    return find_background_window() != nullptr;
}

bool this_program_is_in_background() {
    DWORD process_id = 0;
    HWND window = find_background_window();
    if ( window ) GetWindowThreadProcessId( window, &process_id );
    return process_id == GetCurrentProcessId();
}

bool ask_running_crammy_to_toggle_panel() {
    HWND window = find_background_window();
    return window && PostMessageW( window, show_panel_message(), 0, 0 );
}

UINT next_message() {
    static UINT message = RegisterWindowMessageW( L"CrammyNext" );
    return message;
}

bool ask_running_crammy_for_next() {
    HWND window = find_background_window();
    return window && PostMessageW( window, next_message(), 0, 0 );
}

wstring background_title( const wstring &line, bool next_can_act ) {
    return next_can_act ? line : L"-" + line;
}

wstring taskbar_title( const wstring &desktop, const wstring &line, bool next_can_act ) {
    if ( desktop.empty() ) return background_title( line, next_can_act );
    // The taskbar box has room for about 12 characters, so a longer name ends in an ellipsis.
    wstring name = desktop;
    if ( name.size() > 12 ) name = name.substr( 0, 11 ) + L"\x2026";
    return background_title( name + L"\t" + line, next_can_act );
}

bool all_already_open( const vector<wstring> &files, const vector<recorded_window> &windows ) {
    if ( files.empty() ) return false;
    for ( const wstring &file : files ) {
        bool shown = false;
        for ( const recorded_window &window : windows )
            shown = shown || ( window.alive && lower( window.path ) == lower( file ) );
        if ( !shown ) return false;
    }
    return true;
}

bool watch_has_ended( calendar_date started, calendar_date today ) {
    return started < today;
}

bool ask_running_crammy_to_show_panel() {
    HWND window = find_background_window();
    return window && PostMessageW( window, show_panel_message(), 1, 0 );
}

wstring startup_shortcut_path() {
    wchar_t override_folder[MAX_PATH * 4];
    DWORD length = GetEnvironmentVariableW( L"CRAMMY_STARTUP_FOLDER", override_folder, MAX_PATH * 4 );
    if ( length > 0 && length < MAX_PATH * 4 ) return join_path( wstring( override_folder, length ), L"Crammy.lnk" );

    PWSTR folder = nullptr;
    wstring path;
    if ( SUCCEEDED( SHGetKnownFolderPath( FOLDERID_Startup, 0, nullptr, &folder ) ) ) path = folder;
    CoTaskMemFree( folder );
    return path.empty() ? L"" : join_path( path, L"Crammy.lnk" );
}

wstring sign_in_task_name() {
    wchar_t name[256];
    DWORD length = GetEnvironmentVariableW( L"CRAMMY_SIGN_IN_TASK", name, 256 );
    if ( length > 0 && length < 256 ) return wstring( name, length );
    return L"Crammy";
}

wstring sign_in_task_xml( const wstring &exe, const wstring &user ) {
    wstring command = escape_xml( exe );
    wstring folder = escape_xml( parent_folder( exe ) );
    wstring account = escape_xml( user );
    return L"<?xml version=\"1.0\" encoding=\"UTF-16\"?>\r\n"
           L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\r\n"
           L"  <RegistrationInfo><Description>Starts Crammy in the background when you sign in.</Description></RegistrationInfo>\r\n"
           L"  <Triggers>\r\n"
           L"    <LogonTrigger>\r\n"
           L"      <Enabled>true</Enabled>\r\n"
           L"      <UserId>" + account + L"</UserId>\r\n"
           L"    </LogonTrigger>\r\n"
           L"  </Triggers>\r\n"
           L"  <Principals>\r\n"
           L"    <Principal id=\"Author\">\r\n"
           L"      <UserId>" + account + L"</UserId>\r\n"
           L"      <LogonType>InteractiveToken</LogonType>\r\n"
           L"      <RunLevel>LeastPrivilege</RunLevel>\r\n"
           L"    </Principal>\r\n"
           L"  </Principals>\r\n"
           L"  <Settings>\r\n"
           L"    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>\r\n"
           L"    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>\r\n"
           L"    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\r\n"
           L"    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>\r\n"
           L"    <Priority>5</Priority>\r\n"
           L"    <Enabled>true</Enabled>\r\n"
           L"  </Settings>\r\n"
           L"  <Actions Context=\"Author\">\r\n"
           L"    <Exec>\r\n"
           L"      <Command>" + command + L"</Command>\r\n"
           L"      <Arguments>--background</Arguments>\r\n"
           L"      <WorkingDirectory>" + folder + L"</WorkingDirectory>\r\n"
           L"    </Exec>\r\n"
           L"  </Actions>\r\n"
           L"</Task>\r\n";
}

wstring current_account() {
    wchar_t computer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD computer_length = MAX_COMPUTERNAME_LENGTH + 1;
    wchar_t user[257];
    DWORD user_length = 257;
    if ( !GetComputerNameW( computer, &computer_length ) || !GetUserNameW( user, &user_length ) ) return L"";
    return wstring( computer, computer_length ) + L"\\" + user;
}

wstring registered_sign_in_task_xml() {
    wstring xml;
    with_task_folder( [&]( ITaskFolder *folder ) {
        BSTR name = SysAllocString( sign_in_task_name().c_str() );
        IRegisteredTask *task = nullptr;
        if ( SUCCEEDED( folder->GetTask( name, &task ) ) ) {
            BSTR text = nullptr;
            if ( SUCCEEDED( task->get_Xml( &text ) ) && text ) xml = text;
            SysFreeString( text );
            task->Release();
        }
        SysFreeString( name );
        return true;
    } );
    return xml;
}

bool sign_in_task_exists() {
    return !registered_sign_in_task_xml().empty();
}

bool start_at_sign_in_is_on() {
    return sign_in_task_exists() || shortcut_exists();
}

bool set_start_at_sign_in( bool on ) {
    wstring path = startup_shortcut_path();
    bool shortcut_gone = path.empty() || DeleteFileW( path.c_str() ) || GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND;
    if ( !on ) return remove_sign_in_task() && shortcut_gone;
    if ( register_sign_in_task() ) return true;
    // Without Task Scheduler the Startup folder still starts Crammy, only later
    if ( path.empty() ) return false;
    CreateDirectoryW( parent_folder( path ).c_str(), nullptr );
    return make_shortcut( path );
}

int run_background( bool show_panel_now ) {
    // Held until the process ends, so two launches at the same moment cannot both go on.
    HANDLE lock = CreateMutexW( nullptr, FALSE, L"CrammyBackgroundLock" );
    if ( !lock || GetLastError() == ERROR_ALREADY_EXISTS ) return 0;

    HINSTANCE instance = GetModuleHandleW( nullptr );
    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = background_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = background_class_name;
    RegisterClassW( &window_class );
    // The dash greys the mod's next until the first line is set
    HWND window = CreateWindowExW( 0, background_class_name, L"-Crammy", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr );
    if ( !window ) return 1;
    SetUnhandledExceptionFilter( log_crash );
    std::set_terminate( log_uncaught );
    log_now( L"started, process " + std::to_wstring( GetCurrentProcessId() ) );

    set_start_at_sign_in( current_settings().start_at_sign_in );
    taskbar_created_message = RegisterWindowMessageW( L"TaskbarCreated" );
    add_tray_icon( window );

    UINT modifiers, key;
    shortcut_keys( modifiers, key );
    // Another program may already own the shortcut; the tray icon still works without it.
    RegisterHotKey( window, hotkey_id, modifiers | MOD_NOREPEAT, key );

    start_next_runner( window );
    if ( show_panel_now ) show_panel();
    ask_about_saved_session();
    run_message_loop();
    return 0;
}
