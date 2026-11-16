#pragma once

#include <windows.h>

#include "text.h"
#include "timetable.h"

extern const wchar_t *background_class_name;    // L"CrammyBackground"

UINT show_panel_message();
bool background_is_running();
bool this_program_is_in_background();   // the background window belongs to this process
bool ask_running_crammy_to_toggle_panel();
bool ask_running_crammy_to_show_panel();    // like toggle, but an open panel stays open
UINT next_message();                    // the registered CrammyNext, which the taskbar's next button posts
bool ask_running_crammy_for_next();     // posts next_message() to the CrammyBackground window; false if there is none

// The title Crammy gives its hidden window: the line, with "-" in front when Next would do nothing.
wstring background_title( const wstring &line, bool next_can_act );

// The whole title: the current desktop's name, a tab, then the line, with "-" in front of it all when Next
// would do nothing. The desktop name is cut to 12 characters. With no desktop name it is background_title( line ).
wstring taskbar_title( const wstring &desktop, const wstring &line, bool next_can_act );

struct recorded_window {
    wstring path;   // the file the window shows
    bool alive;     // the window still exists
};
// True when every file is shown by a recorded window that still exists. Paths are compared
// without regard to case. An empty list of files gives false.
bool all_already_open( const vector<wstring> &files, const vector<recorded_window> &windows );

// True once the day has changed since the watch started, so watches end at midnight.
bool watch_has_ended( calendar_date started, calendar_date today );

bool set_start_at_sign_in( bool on );
wstring startup_shortcut_path();    // CRAMMY_STARTUP_FOLDER, when set, replaces the Startup folder
int run_background( bool show_panel_now );

// The hidden window's procedure, declared so the tests can make a window that uses it.
LRESULT CALLBACK background_window_proc( HWND window, UINT message, WPARAM wparam, LPARAM lparam );

// background log.txt, next to crammy.exe: when the background copy started and why it stopped.
wstring background_log_path();

// One line of the log: "2026-11-15 20:01:19  text", with two spaces after the time.
wstring log_line( const SYSTEMTIME &at, const wstring &text );

// What a crash is logged as: "crashed with exception 0xC0000005 at crammy.exe+0x1A2B3".
wstring crash_text( DWORD code, unsigned long long offset );

// Adds line and a line break to the file at path, keeping only the last keep lines.
void append_log( const wstring &path, const wstring &line, size_t keep );
