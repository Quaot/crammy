#pragma once

#include <windows.h>

#include "text.h"

enum class program { pdfcraft, acrobat, notepad_pp, explorer, chrome };

// Where each program's .exe is, for starting it and for its icon
const wchar_t *program_file( program app );

struct planned_window {
    wstring side;           // left, right or full
    program app;
    vector<wstring> paths;  // PDFs for PdfCraft, one file for Notepad++, one folder for Explorer, addresses for Chrome
    wstring control_file;   // PdfCraft only: when set, the window starts with --control and this file
    wstring desktop;        // the desktop this window goes to, or "" for the desktop showing
    int split = 50;         // the left side's share of the screen width in percent
    bool has_frame = false; // when true, the window goes exactly to frame and is never snapped
    RECT frame{};
    int page = 0;           // PdfCraft and Acrobat: the page to open at; 0 for the program's own choice
};

#include <windows.h>

// The part of the screen each planned window should fill, in plan order. Windows that share a
// side split it into rows, top to bottom in plan order, but only with windows going to the same
// desktop: windows on different desktops never share rows.
vector<RECT> window_targets( const vector<planned_window> &plan, const RECT &screen );

#include <initializer_list>

#include <windows.h>

// Presses the keys together, then lets them go in reverse order.
void press_keys( std::initializer_list<unsigned short> keys );

DWORD start_process( wstring command_line );
wstring quoted( const wstring &text );

// The command line that starts PdfCraft on these PDFs in a window of its own, in Read mode,
// fitted to width. control_file, when not empty, adds --control and that file. A page above 0
// adds --page and that number.
wstring pdfcraft_command( const vector<wstring> &paths, const wstring &control_file, int page = 0 );

// The command line that starts Acrobat on one PDF in a new window, fitted to width, at the page
// when it is above 0.
wstring acrobat_command( const wstring &path, int page = 0 );

// The command line that starts Chrome on these addresses, as tabs in a new window.
wstring chrome_command( const vector<wstring> &addresses );

// The command line that starts Notepad++ on one file, in a window of its own.
wstring notepad_command( const wstring &path );
vector<HWND> top_windows();
wstring title_of( HWND hwnd );
bool bring_to_front( HWND hwnd );

// Starts every window at once, then moves each one to its part of the screen. A window with a
// desktop is moved there as soon as it appears and sized without keys or focus; the others are
// snapped afterwards. Returns a message for each window that never appeared.
vector<wstring> open_windows( const vector<planned_window> &plan );

struct opened_window {
    HWND window;
    DWORD process_id;   // the window's process, so a reused handle is never taken for it
    wstring path;       // one of the files the window shows
    program app;
    wstring side;
};

// open_windows, and also says which window shows which file: one entry for each path of each
// window that appeared. problems gets a message for each window that never appeared.
vector<opened_window> open_windows_recorded( const vector<planned_window> &plan, vector<wstring> &problems );

// Opens the windows on the desktop with this name while the desktop showing stays the same.
// Each window is moved there as soon as it appears, then sized to its side without pressing
// keys or taking the focus. problems also gets a message for each window that could not move.
vector<opened_window> open_windows_on_desktop( const vector<planned_window> &plan, const wstring &desktop,
                                              vector<wstring> &problems );

// Opens one window over the frame old_window has now. It only moves the window there: it
// presses no keys and leaves the focus where it is.
vector<opened_window> open_window_in_place_of( const planned_window &window, HWND old_window, vector<wstring> &problems );

// True when the window still exists and still belongs to that process.
bool still_ours( HWND window, DWORD process_id );
