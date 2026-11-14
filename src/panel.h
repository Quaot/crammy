#pragma once

#include <windows.h>

#include "panel_model.h"

// Opens the panel above the bottom-left corner of the taskbar, or closes it when it is open.
void toggle_panel();

// Opens the panel when it is not open; an open panel stays as it is.
void show_panel();

// Brings the main window forward, or opens it when it is not open.
void show_setups();

// The height draw_panel needs for this content.
int panel_height( const today_section &today, const vector<desktop_tile> &tiles, UINT dpi );

void draw_panel( HDC dc, int width, int height, const today_section &today, const vector<desktop_tile> &tiles,
                 const wstring &date_text, UINT dpi );

// Opens the panel with a problem written under it.
void show_panel_problem( const wstring &problem );

// The courses in the timetable that have a folder in Downloads.
vector<wstring> known_courses( const vector<class_block> &blocks );

// Creates the class's desktop when the click says so, then switches to it. Returns false when the
// switch failed; problem says what went wrong.
bool go_to_class_desktop( const class_click &click, wstring &problem );
