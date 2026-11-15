#pragma once

#include <windows.h>

#include "text.h"
#include "tiling.h"

// Saving every study window before the PC shuts down, and putting them back afterwards.
// Study windows are every PdfCraft, Notepad++ and Acrobat window, whoever opened them.

struct saved_window {
    wstring app;            // "pdfcraft", "notepad++" or "acrobat"
    wstring desktop;        // the desktop's name, or "" when it is not known
    RECT frame{};           // the visible frame on the screen
    vector<wstring> paths;  // the files it shows, in order
};

struct saved_session {
    bool asked = false;     // the sign-in question about it has been asked
    vector<saved_window> windows;
};

wstring write_session( const saved_session &session );
saved_session read_session( const wstring &text );

// True for a top-level study window: Notepad++'s main window (class "Notepad++"), Acrobat's
// document window (class "AcrobatSDIWindow"), or any window of pdfcraft.exe. exe_name is
// the file name of the window's program; compared without regard to case.
bool is_study_window( const wstring &exe_name, const wstring &class_name );

// The app name used in the session file for that program: "pdfcraft", "notepad++",
// "acrobat", or "" for any other program.
wstring app_of( const wstring &exe_name );

// The PDFs named on a program's command line, in order: every argument that ends in ".pdf".
vector<wstring> pdfs_in_command_line( const wstring &command_line );

// Saves every file each window has open, records each window, closes it, and returns what
// was recorded. A window that cannot be closed safely is recorded but left open, and
// problems says why. Waits up to 5 seconds for the windows to go.
vector<saved_window> save_and_close( const vector<HWND> &windows, vector<wstring> &problems );

// save_and_close on every study window on every desktop, then writes the session (asked = no)
// to path. Returns the number of windows recorded.
int save_and_close_all( const wstring &path, vector<wstring> &problems );

wstring session_path();     // "last session.txt" next to profiles.txt

// The windows to open for a saved session: one per saved window that still has a file, each
// going to its own desktop and frame. An Acrobat window gives one window per file.
vector<planned_window> reopen_plan( const saved_session &session );

// Opens the session at path on its desktops without switching, and marks it asked.
// Returns problems to show.
vector<wstring> reopen_session( const wstring &path );
