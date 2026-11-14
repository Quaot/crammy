#pragma once

#include <windows.h>

#include <functional>

#include "text.h"

// Next and what it keeps for this run: the windows it opened and the watches on newest slides.
// It also keeps the lecture decks opened today; only their place is saved, in places.txt.

// Remembers the hidden background window, sets its title and starts the timers.
void start_next_runner( HWND background_window );

// True while Next or next deck is opening windows, or while a tick list is open.
bool next_is_busy();

// Runs work with Next busy, then stops being busy and drops the presses of Next that arrived
// meanwhile, so they do not run afterwards.
void run_as_busy( const std::function<void()> &work );

// Called for every WM_TIMER of the background window; ignores timers that are not Next's.
void on_next_timer( UINT_PTR timer_id );

// Recomputes the taskbar line and sets it as the background window's title.
void refresh_line();

// Goes to the next class's desktop and opens its setup. Ignored while a Next or next deck is busy.
void run_next();

// "next deck" or "last deck" while the next class has decks open today, otherwise "".
wstring next_deck_label();

// Reopens the class's PdfCraft window with today's decks and the deck after the place as tabs,
// closes the old window and saves the place.
void run_next_deck();
