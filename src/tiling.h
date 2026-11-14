#pragma once

#include "text.h"

enum class program { pdfcraft, acrobat, notepad_pp, explorer };

// Where each program's .exe is, for starting it and for its icon
const wchar_t *program_file( program app );

struct planned_window {
    wstring side;           // left, right or full
    program app;
    vector<wstring> paths;  // PDFs for PdfCraft, one file for Notepad++, one folder for Explorer
    wstring control_file;   // PdfCraft only: when set, the window starts with --control and this file
};

#include <initializer_list>

#include <windows.h>

// Presses the keys together, then lets them go in reverse order.
void press_keys( std::initializer_list<unsigned short> keys );

DWORD start_process( wstring command_line );
wstring quoted( const wstring &text );
vector<HWND> top_windows();
wstring title_of( HWND hwnd );
bool bring_to_front( HWND hwnd );

// Starts every window, then moves each one to its part of the screen.
// Returns a message for each window that never appeared.
vector<wstring> open_windows( const vector<planned_window> &plan );

struct opened_window {
    HWND window;
    wstring path;       // one of the files the window shows
    program app;
    wstring side;
};

// open_windows, and also says which window shows which file: one entry for each path of each
// window that appeared. problems gets a message for each window that never appeared.
vector<opened_window> open_windows_recorded( const vector<planned_window> &plan, vector<wstring> &problems );
