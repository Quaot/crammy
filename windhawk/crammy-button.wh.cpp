// ==WindhawkMod==
// @id              crammy-button
// @name            Crammy button
// @description     Replaces the Widgets button with Crammy's line and a next button
// @version         0.8
// @author          Justin Gu
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -lshell32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Crammy button

Hides the Widgets button at the left end of the taskbar and puts Crammy's line
in its place, in two small rows like the weather's: the next class on top, such
as `MATH 117 Tutorial`, and where and when below it, such as
`Mon 08:30 · DWE 1515`. To the left of the rows, the name of the virtual
desktop showing, such as `150`, is written a little larger, with a thin line
after it. The faint `›` to the right of the rows is the next button. Everything
is drawn in the taskbar's own typeface and text colour, and the buttons stay
clear when pointed at or pressed. The line is read from Crammy every half
second, so the name changes soon after a switch to another desktop.

A click on the rows opens the Crammy panel. A click on `▸` goes to that class's
desktop and opens its slides, as Next does in Crammy. The next button is greyed
when Next has nothing to open for the class.

When Crammy is not running, the top row reads `Crammy is not running` with the
next button greyed and no desktop name, and a click on it starts
`crammy.exe --panel`.

The whole line, the desktop name and `›` included, is kept within the width set as
`line_width`, 270 by default, so it stays in the space the Widgets button had,
before the Start button. A row that does not fit ends in an ellipsis. A width
saved by an older version stays until it is changed in the settings.

Switch the mod off to bring the Widgets button back.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- crammy_path: C:\Users\Justin\Desktop\Projects\Crammy\crammy.exe
  $name: Path to crammy.exe
  $description: The program to start when Crammy is not running.
- line_width: 270
  $name: Width of the line
  $description: The widest the line may be, the next button included, in pixels at 100% scaling. Lower it if the line reaches the Start button.
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#undef GetCurrentTime

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/base.h>

using namespace winrt::Windows::UI::Xaml;

const wchar_t line_name[] = L"CrammyLine";
const wchar_t not_running_text[] = L"Crammy is not running";
const int default_line_width = 270;

struct placed_line {
    winrt::weak_ref<Controls::Grid> panel;
    Controls::Primitives::ButtonBase::Click_revoker text_click;
    Controls::Primitives::ButtonBase::Click_revoker next_click;
    winrt::weak_ref<Controls::Border> box;
    winrt::weak_ref<Controls::TextBlock> desktop;
    winrt::weak_ref<Controls::TextBlock> top;
    winrt::weak_ref<Controls::TextBlock> bottom;
    winrt::weak_ref<Controls::Button> next;
};

std::wstring g_crammy_path;
std::atomic<int> g_line_width{ default_line_width };
std::atomic<bool> g_unloading;
std::atomic<bool> g_taskbar_view_hooked;
std::vector<placed_line> g_lines;
// One Windows timer on the taskbar's thread refreshes every line. A XAML
// DispatcherTimer did not tick in the taskbar's XAML host, so the line only
// changed when the mod was reloaded.
UINT_PTR g_refresh_timer = 0;
std::vector<winrt::weak_ref<FrameworkElement>> g_hidden_widgets;
FrameworkElement::Loaded_revoker g_loaded_revoker;

void show_panel() {
    static const UINT show_message = RegisterWindowMessageW( L"CrammyShowPanel" );
    // Explorer has the last input, so it can let Crammy's panel take the focus.
    if ( HWND crammy_window = FindWindowW( L"CrammyBackground", nullptr ) ) {
        DWORD crammy_process = 0;
        GetWindowThreadProcessId( crammy_window, &crammy_process );
        AllowSetForegroundWindow( crammy_process );
        PostMessageW( crammy_window, show_message, 0, 0 );
        return;
    }
    AllowSetForegroundWindow( ASFW_ANY );
    size_t slash = g_crammy_path.find_last_of( L'\\' );
    std::wstring folder = slash == std::wstring::npos ? L"" : g_crammy_path.substr( 0, slash );
    ShellExecuteW( nullptr, L"open", g_crammy_path.c_str(), L"--panel",
                   folder.empty() ? nullptr : folder.c_str(), SW_SHOWNORMAL );
}

void press_next() {
    static const UINT next_message = RegisterWindowMessageW( L"CrammyNext" );
    if ( HWND crammy_window = FindWindowW( L"CrammyBackground", nullptr ) ) {
        // Next may switch desktops and open windows, so Crammy may take the focus.
        DWORD crammy_process = 0;
        GetWindowThreadProcessId( crammy_window, &crammy_process );
        AllowSetForegroundWindow( crammy_process );
        PostMessageW( crammy_window, next_message, 0, 0 );
    }
}

void set_text( Controls::TextBlock text, const std::wstring &value ) {
    if ( text.Text() != value ) {
        text.Text( value );
    }
}

void set_shown( UIElement element, bool shown ) {
    Visibility wanted = shown ? Visibility::Visible : Visibility::Collapsed;
    if ( element.Visibility() != wanted ) {
        element.Visibility( wanted );
    }
}

// Crammy keeps the line as the title of its CrammyBackground window: the
// desktop's name, a tab, then the line, with a newline between its two rows.
// A title starting with "-" means Next has nothing to open for that class.
// A title with no tab has no desktop name, and the box is hidden.
void refresh_line( Controls::Border box, Controls::TextBlock desktop, Controls::TextBlock top,
                   Controls::TextBlock bottom, Controls::Button next ) {
    std::wstring line = not_running_text;
    std::wstring desktop_name;
    bool can_next = false;
    if ( HWND crammy_window = FindWindowW( L"CrammyBackground", nullptr ) ) {
        // The window belongs to another process, so GetWindowTextW reads the
        // stored title and sends Crammy no message; a hung Crammy cannot block it.
        wchar_t title[256] = {};
        GetWindowTextW( crammy_window, title, ARRAYSIZE( title ) );
        line = title;
        can_next = true;
        if ( !line.empty() && line[0] == L'-' ) {
            line.erase( 0, 1 );
            can_next = false;
        }
        size_t tab = line.find( L'\t' );
        if ( tab != std::wstring::npos ) {
            desktop_name = line.substr( 0, tab );
            line.erase( 0, tab + 1 );
        }
        if ( line.empty() ) {
            // The window may have closed after FindWindowW found it.
            line = L"Crammy";
            can_next = false;
        }
    }
    set_text( desktop, desktop_name );
    set_shown( box, !desktop_name.empty() );
    // A line of one row, such as "No classes", leaves the bottom row hidden.
    size_t newline = line.find( L'\n' );
    std::wstring top_row = line.substr( 0, newline );
    std::wstring bottom_row = newline == std::wstring::npos ? L"" : line.substr( newline + 1 );
    set_text( top, top_row );
    set_text( bottom, bottom_row );
    set_shown( bottom, !bottom_row.empty() );
    if ( next.IsEnabled() != can_next ) {
        next.IsEnabled( can_next );
    }
}

// Runs on the taskbar's thread every half second: SetTimer with no window calls
// it from the message loop of the thread that set it, which is the taskbar's.
void CALLBACK refresh_all_lines( HWND, UINT, UINT_PTR, DWORD ) {
    if ( g_unloading ) {
        return;
    }
    try {
        for ( auto &placed : g_lines ) {
            auto box = placed.box.get();
            auto desktop = placed.desktop.get();
            auto top = placed.top.get();
            auto bottom = placed.bottom.get();
            auto next = placed.next.get();
            if ( box && desktop && top && bottom && next ) {
                refresh_line( box, desktop, top, bottom, next );
            }
        }
    } catch ( ... ) {
        Wh_Log( L"Refreshing the line failed" );
    }
}

FrameworkElement find_child( FrameworkElement element, PCWSTR name, bool by_class ) {
    int count = Media::VisualTreeHelper::GetChildrenCount( element );
    for ( int i = 0; i < count; i++ ) {
        auto child = Media::VisualTreeHelper::GetChild( element, i ).try_as<FrameworkElement>();
        if ( child && ( by_class ? winrt::get_class_name( child ) == name : child.Name() == name ) ) {
            return child;
        }
    }
    return nullptr;
}

FrameworkElement find_parent_by_class( FrameworkElement element, PCWSTR class_name ) {
    while ( element ) {
        element = Media::VisualTreeHelper::GetParent( element ).try_as<FrameworkElement>();
        if ( element && winrt::get_class_name( element ) == class_name ) {
            return element;
        }
    }
    return nullptr;
}

// The buttons keep the taskbar's own typeface and text colour: only the
// background is cleared, as on the taskbar's own buttons.
Controls::Button make_button( UIElement content, double side_padding ) {
    Controls::Button button;
    button.Content( content );
    button.Height( 40 );
    button.Padding( Thickness{ side_padding, 0, side_padding, 0 } );
    button.BorderThickness( Thickness{ 0, 0, 0, 0 } );
    Media::SolidColorBrush clear( winrt::Windows::UI::Color{ 0, 0, 0, 0 } );
    button.Background( clear );
    // The button's own pointer-over and pressed colours would turn it grey;
    // these keys replace them, so it stays clear in every state.
    for ( PCWSTR key : { L"ButtonBackground", L"ButtonBackgroundPointerOver", L"ButtonBackgroundPressed",
                         L"ButtonBackgroundDisabled", L"ButtonBorderBrush", L"ButtonBorderBrushPointerOver",
                         L"ButtonBorderBrushPressed", L"ButtonBorderBrushDisabled" } ) {
        button.Resources().Insert( winrt::box_value( key ), clear );
    }
    button.VerticalAlignment( VerticalAlignment::Center );
    return button;
}

// One of the two rows, in a smaller font than the taskbar's, as the weather's are.
Controls::TextBlock make_row() {
    Controls::TextBlock row;
    row.FontSize( 12 );
    row.TextWrapping( TextWrapping::NoWrap );
    row.TextTrimming( TextTrimming::CharacterEllipsis );
    return row;
}

// The desktop's name, a little larger than the rows and in the taskbar's own
// colour, with a thin line after it to set it apart from the class. It starts
// hidden, until a title with a name is read.
Controls::Border make_desktop_box( Controls::TextBlock name ) {
    name.FontSize( 15 );
    name.TextWrapping( TextWrapping::NoWrap );
    name.VerticalAlignment( VerticalAlignment::Center );
    Controls::Border box;
    box.Child( name );
    // The thin line on the right, in the text colour at low opacity
    box.BorderBrush( Media::SolidColorBrush( winrt::Windows::UI::Color{ 0x50, 0x80, 0x80, 0x80 } ) );
    box.BorderThickness( Thickness{ 0, 0, 1, 0 } );
    box.Padding( Thickness{ 2, 4, 8, 4 } );
    box.Margin( Thickness{ 0, 0, 0, 0 } );
    box.VerticalAlignment( VerticalAlignment::Center );
    box.Visibility( Visibility::Collapsed );
    return box;
}

void add_line( Controls::Grid root_grid ) {
    // The taskbar can drop its tree, for example when a monitor goes away.
    std::erase_if( g_lines, []( placed_line &placed ) {
        return !placed.panel.get();
    } );
    // A line already in the tree is either this load's, which stays, or one left
    // behind by an older load whose unload could not reach the taskbar thread.
    // A left-behind line no longer updates, so it is taken out and replaced.
    auto children = root_grid.Children();
    for ( uint32_t i = children.Size(); i > 0; --i ) {
        auto element = children.GetAt( i - 1 ).try_as<FrameworkElement>();
        if ( !element || element.Name() != line_name ) {
            continue;
        }
        bool ours = std::any_of( g_lines.begin(), g_lines.end(),
                                 [&]( placed_line &placed ) { return placed.panel.get() == element; } );
        if ( ours ) {
            return;
        }
        Wh_Log( L"Replacing a line left behind by an older load" );
        children.RemoveAt( i - 1 );
    }
    Controls::TextBlock desktop;
    Controls::Border box = make_desktop_box( desktop );

    Controls::TextBlock top = make_row();
    Controls::TextBlock bottom = make_row();
    Controls::StackPanel rows;
    rows.Orientation( Controls::Orientation::Vertical );
    rows.VerticalAlignment( VerticalAlignment::Center );
    rows.Children().Append( top );
    rows.Children().Append( bottom );
    Controls::Button text_button = make_button( rows, 6 );

    Controls::TextBlock next_text;
    next_text.Text( L"\u203A" );
    next_text.FontSize( 18 );
    next_text.Opacity( 0.55 );
    next_text.VerticalAlignment( VerticalAlignment::Center );
    Controls::Button next_button = make_button( next_text, 4 );
    Automation::AutomationProperties::SetName( next_button, L"Next class" );

    // The whole line, padding, box and next button included, stays within
    // line_width: the rows take what the box and the next button leave and
    // are cut short.
    Controls::ColumnDefinition box_column;
    box_column.Width( GridLengthHelper::Auto() );
    Controls::ColumnDefinition rows_column;
    rows_column.Width( GridLengthHelper::FromValueAndType( 1, GridUnitType::Star ) );
    Controls::ColumnDefinition next_column;
    next_column.Width( GridLengthHelper::Auto() );
    Controls::Grid panel;
    panel.Name( line_name );
    panel.ColumnDefinitions().Append( box_column );
    panel.ColumnDefinitions().Append( rows_column );
    panel.ColumnDefinitions().Append( next_column );
    panel.Padding( Thickness{ 4, 0, 0, 0 } );
    panel.MaxWidth( g_line_width );
    panel.HorizontalAlignment( HorizontalAlignment::Left );
    panel.VerticalAlignment( VerticalAlignment::Center );
    Controls::Grid::SetColumn( box, 0 );
    Controls::Grid::SetColumn( text_button, 1 );
    Controls::Grid::SetColumn( next_button, 2 );
    panel.Children().Append( box );
    panel.Children().Append( text_button );
    panel.Children().Append( next_button );

    refresh_line( box, desktop, top, bottom, next_button );

    placed_line placed;
    placed.panel = winrt::make_weak( panel );
    placed.text_click = text_button.Click( winrt::auto_revoke, []( auto &&, auto && ) {
        try {
            show_panel();
        } catch ( ... ) {
            Wh_Log( L"Opening the panel failed" );
        }
    } );
    placed.next_click = next_button.Click( winrt::auto_revoke, []( auto &&, auto && ) {
        try {
            press_next();
        } catch ( ... ) {
            Wh_Log( L"Sending Next failed" );
        }
    } );
    placed.box = winrt::make_weak( box );
    placed.desktop = winrt::make_weak( desktop );
    placed.top = winrt::make_weak( top );
    placed.bottom = winrt::make_weak( bottom );
    placed.next = winrt::make_weak( next_button );
    // Reading the title sends Crammy no message, so every half second is cheap,
    // and the desktop name follows a switch soon after it.
    if ( !g_refresh_timer ) {
        g_refresh_timer = SetTimer( nullptr, 0, 500, refresh_all_lines );
        if ( !g_refresh_timer ) {
            Wh_Log( L"Could not start the refresh timer, error %u", GetLastError() );
        }
    }
    // Added last, so an error above leaves nothing in the taskbar's tree.
    root_grid.Children().Append( panel );
    g_lines.push_back( std::move( placed ) );
}

bool replace_widgets_unguarded( FrameworkElement widgets ) {
    auto frame = find_parent_by_class( widgets, L"Taskbar.TaskbarFrame" );
    auto root_grid = frame ? find_child( frame, L"RootGrid", false ).try_as<Controls::Grid>() : nullptr;
    if ( !root_grid ) {
        return false;
    }
    if ( Automation::AutomationProperties::GetAutomationId( widgets ) != L"WidgetsButton" ) {
        return true;
    }
    widgets.Visibility( Visibility::Collapsed );
    bool known = false;
    for ( auto &hidden : g_hidden_widgets ) {
        known = known || hidden.get() == widgets;
    }
    if ( !known ) {
        g_hidden_widgets.push_back( winrt::make_weak( widgets ) );
    }
    add_line( root_grid );
    return true;
}

// Returns false when the Widgets button is not in the taskbar's tree yet.
// This runs inside the taskbar's own code, so no WinRT error may leave it.
bool replace_widgets( FrameworkElement widgets ) {
    try {
        return replace_widgets_unguarded( widgets );
    } catch ( ... ) {
        Wh_Log( L"Placing the line failed" );
        return true;
    }
}

void restore_all() {
    g_loaded_revoker.revoke();
    if ( g_refresh_timer ) {
        KillTimer( nullptr, g_refresh_timer );
        g_refresh_timer = 0;
    }
    for ( auto &placed : g_lines ) {
        placed.text_click.revoke();
        placed.next_click.revoke();
        auto line = placed.panel.get();
        auto parent = line ? Media::VisualTreeHelper::GetParent( line ).try_as<Controls::Panel>() : nullptr;
        uint32_t index = 0;
        if ( parent && parent.Children().IndexOf( line, index ) ) {
            parent.Children().RemoveAt( index );
        }
    }
    g_lines.clear();
    for ( auto &hidden : g_hidden_widgets ) {
        if ( auto widgets = hidden.get() ) {
            widgets.Visibility( Visibility::Visible );
        }
    }
    g_hidden_widgets.clear();
}

void apply_all( XamlRoot xaml_root ) {
    FrameworkElement element = xaml_root.Content().try_as<FrameworkElement>();
    if ( element && ( element = find_child( element, L"Taskbar.TaskbarFrame", true ) ) &&
         ( element = find_child( element, L"RootGrid", false ) ) &&
         ( element = find_child( element, L"TaskbarFrameRepeater", false ) ) ) {
        int count = Media::VisualTreeHelper::GetChildrenCount( element );
        for ( int i = 0; i < count; i++ ) {
            auto child = Media::VisualTreeHelper::GetChild( element, i ).try_as<FrameworkElement>();
            if ( child && winrt::get_class_name( child ) == L"Taskbar.AugmentedEntryPointButton" ) {
                replace_widgets( child );
            }
        }
    }
}

using AugmentedEntryPointButton_UpdateButtonPadding_t = void( WINAPI * )( void *pThis );
AugmentedEntryPointButton_UpdateButtonPadding_t AugmentedEntryPointButton_UpdateButtonPadding_Original;
void WINAPI AugmentedEntryPointButton_UpdateButtonPadding_Hook( void *pThis ) {
    AugmentedEntryPointButton_UpdateButtonPadding_Original( pThis );
    if ( g_unloading ) {
        return;
    }
    FrameworkElement widgets = nullptr;
    ( (IUnknown **)pThis )[1]->QueryInterface( winrt::guid_of<FrameworkElement>(), winrt::put_abi( widgets ) );
    if ( !widgets || replace_widgets( widgets ) ) {
        return;
    }
    g_loaded_revoker = widgets.Loaded( winrt::auto_revoke, []( auto const &sender, auto && ) {
        if ( auto element = sender.template try_as<FrameworkElement>(); element && !g_unloading ) {
            replace_widgets( element );
        }
    } );
}

// The taskbar's XamlRoot is reached the way m417z's "Taskbar tray icon
// spacing and grid" mod does it, through taskbar.dll's TaskbarHost.
void *CTaskBand_ITaskListWndSite_vftable;
using CTaskBand_GetTaskbarHost_t = void *( WINAPI * )( void *pThis, void **result );
CTaskBand_GetTaskbarHost_t CTaskBand_GetTaskbarHost_Original;
void *TaskbarHost_FrameHeight_Original;
using std__Ref_count_base__Decref_t = void( WINAPI * )( void *pThis );
std__Ref_count_base__Decref_t std__Ref_count_base__Decref_Original;

XamlRoot taskbar_xaml_root( HWND taskbar_window ) {
    HWND task_switch_window = (HWND)GetProp( taskbar_window, L"TaskbandHWND" );
    if ( !task_switch_window ) {
        return nullptr;
    }
    void *task_band = (void *)GetWindowLongPtr( task_switch_window, 0 );
    for ( int i = 0; *(void **)task_band != CTaskBand_ITaskListWndSite_vftable; i++ ) {
        if ( i == 20 ) {
            return nullptr;
        }
        task_band = (void **)task_band + 1;
    }
    void *host[2]{};
    CTaskBand_GetTaskbarHost_Original( task_band, host );
    if ( !host[0] && !host[1] ) {
        return nullptr;
    }
    // The offset of the frame inside TaskbarHost is read from FrameHeight:
    // sub rsp,28 then add rcx,<offset>.
    size_t frame_offset = 0x10;
    const BYTE *code = (const BYTE *)TaskbarHost_FrameHeight_Original;
    if ( code[0] == 0x48 && code[1] == 0x83 && code[2] == 0xEC && code[4] == 0x48 &&
         code[5] == 0x83 && code[6] == 0xC1 && code[7] <= 0x7F ) {
        frame_offset = code[7];
    }
    FrameworkElement frame = nullptr;
    ( *(IUnknown **)( (BYTE *)host[0] + frame_offset ) )
        ->QueryInterface( winrt::guid_of<FrameworkElement>(), winrt::put_abi( frame ) );
    auto result = frame ? frame.XamlRoot() : nullptr;
    std__Ref_count_base__Decref_Original( host[1] );
    return result;
}

HWND find_taskbar_window() {
    HWND found = nullptr;
    EnumWindows( []( HWND window, LPARAM param ) -> BOOL {
        DWORD process_id = 0;
        wchar_t class_name[32];
        if ( GetWindowThreadProcessId( window, &process_id ) && process_id == GetCurrentProcessId() &&
             GetClassNameW( window, class_name, ARRAYSIZE( class_name ) ) &&
             _wcsicmp( class_name, L"Shell_TrayWnd" ) == 0 ) {
            *(HWND *)param = window;
            return FALSE;
        }
        return TRUE;
    }, (LPARAM)&found );
    return found;
}

// XAML objects may only be touched from the taskbar's own thread.
bool run_on_taskbar_thread( void ( *proc )( HWND ) ) {
    static const UINT run_message = RegisterWindowMessageW( L"Windhawk_RunFromWindowThread_" WH_MOD_ID );
    struct run_request {
        void ( *proc )( HWND );
        HWND window;
    };
    HWND window = find_taskbar_window();
    if ( !window ) {
        Wh_Log( L"No taskbar window in this process" );
        return false;
    }
    DWORD thread_id = GetWindowThreadProcessId( window, nullptr );
    // Already on the taskbar's thread, as Windhawk sometimes is when it unloads the mod: run it here
    if ( thread_id == GetCurrentThreadId() ) {
        proc( window );
        return true;
    }
    HHOOK hook = SetWindowsHookExW( WH_CALLWNDPROC, []( int code, WPARAM wparam, LPARAM lparam ) -> LRESULT {
        const CWPSTRUCT *message = (const CWPSTRUCT *)lparam;
        if ( code == HC_ACTION && message->message == run_message ) {
            auto *request = (run_request *)message->lParam;
            request->proc( request->window );
        }
        return CallNextHookEx( nullptr, code, wparam, lparam );
    }, nullptr, thread_id );
    if ( !hook ) {
        Wh_Log( L"Could not hook the taskbar thread, error %u", GetLastError() );
        return false;
    }
    run_request request{ proc, window };
    SendMessageW( window, run_message, 0, (LPARAM)&request );
    UnhookWindowsHookEx( hook );
    return true;
}

// These run inside a message sent to the taskbar, so no WinRT error may leave them.
void apply_on_taskbar( HWND window ) {
    try {
        if ( auto xaml_root = taskbar_xaml_root( window ) ) {
            apply_all( xaml_root );
        }
    } catch ( ... ) {
        Wh_Log( L"Placing the line failed" );
    }
}

void restore_on_taskbar( HWND ) {
    try {
        restore_all();
    } catch ( ... ) {
        Wh_Log( L"Removing the line failed" );
    }
}

void load_settings() {
    PCWSTR path = Wh_GetStringSetting( L"crammy_path" );
    g_crammy_path = path;
    Wh_FreeStringSetting( path );
    int width = Wh_GetIntSetting( L"line_width" );
    g_line_width = width > 0 ? width : default_line_width;
}

HMODULE taskbar_view_module() {
    HMODULE module = GetModuleHandleW( L"Taskbar.View.dll" );
    return module ? module : GetModuleHandleW( L"ExplorerExtensions.dll" );
}

bool hook_taskbar_view( HMODULE module ) {
    WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {
            { LR"(protected: virtual void __cdecl winrt::Taskbar::implementation::AugmentedEntryPointButton::UpdateButtonPadding(void))" },
            &AugmentedEntryPointButton_UpdateButtonPadding_Original,
            AugmentedEntryPointButton_UpdateButtonPadding_Hook,
        },
    };
    return WindhawkUtils::HookSymbols( module, hooks, ARRAYSIZE( hooks ) );
}

bool find_taskbar_symbols() {
    HMODULE module = LoadLibraryExW( L"taskbar.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
    if ( !module ) {
        return false;
    }
    WindhawkUtils::SYMBOL_HOOK symbols[] = {
        { { LR"(const CTaskBand::`vftable'{for `ITaskListWndSite'})" }, &CTaskBand_ITaskListWndSite_vftable },
        { { LR"(public: virtual class std::shared_ptr<class TaskbarHost> __cdecl CTaskBand::GetTaskbarHost(void)const )" }, &CTaskBand_GetTaskbarHost_Original },
        { { LR"(public: int __cdecl TaskbarHost::FrameHeight(void)const )" }, &TaskbarHost_FrameHeight_Original },
        { { LR"(public: void __cdecl std::_Ref_count_base::_Decref(void))" }, &std__Ref_count_base__Decref_Original },
    };
    return WindhawkUtils::HookSymbols( module, symbols, ARRAYSIZE( symbols ) );
}

using LoadLibraryExW_t = decltype( &LoadLibraryExW );
LoadLibraryExW_t LoadLibraryExW_Original;
HMODULE WINAPI LoadLibraryExW_Hook( LPCWSTR file_name, HANDLE file, DWORD flags ) {
    HMODULE module = LoadLibraryExW_Original( file_name, file, flags );
    if ( module && !g_taskbar_view_hooked && module == taskbar_view_module() &&
         !g_taskbar_view_hooked.exchange( true ) && hook_taskbar_view( module ) ) {
        Wh_ApplyHookOperations();
    }
    return module;
}

BOOL Wh_ModInit() {
    load_settings();
    if ( !find_taskbar_symbols() ) {
        return FALSE;
    }
    if ( HMODULE module = taskbar_view_module() ) {
        g_taskbar_view_hooked = true;
        return hook_taskbar_view( module );
    }
    auto kernel_base_load = (LoadLibraryExW_t)GetProcAddress( GetModuleHandleW( L"kernelbase.dll" ), "LoadLibraryExW" );
    WindhawkUtils::SetFunctionHook( kernel_base_load, LoadLibraryExW_Hook, &LoadLibraryExW_Original );
    return TRUE;
}

void Wh_ModAfterInit() {
    if ( !g_taskbar_view_hooked ) {
        if ( HMODULE module = taskbar_view_module() ) {
            if ( !g_taskbar_view_hooked.exchange( true ) && hook_taskbar_view( module ) ) {
                Wh_ApplyHookOperations();
            }
        }
    }
    run_on_taskbar_thread( apply_on_taskbar );
}

void Wh_ModBeforeUninit() {
    g_unloading = true;
    // The timer can only be stopped on the taskbar thread. If that thread
    // cannot be reached, the timer may still fire after the mod is unloaded.
    // This is rare and accepted.
    if ( !run_on_taskbar_thread( restore_on_taskbar ) ) {
        Wh_Log( L"Could not reach the taskbar thread to remove the line" );
    }
}

void Wh_ModSettingsChanged() {
    load_settings();
    run_on_taskbar_thread( restore_on_taskbar );
    run_on_taskbar_thread( apply_on_taskbar );
}
