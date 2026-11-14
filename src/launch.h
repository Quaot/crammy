#pragma once

#include "files.h"
#include "sequence.h"
#include "tiling.h"

// The PDFs a pick item offers to tick. A sequence item offers its numbered decks in section order.
vector<pdf_file> pick_files( const layout_item &item );

// The place saved for the setup in places.txt, or "" when there is none
wstring saved_place( const wstring &title );

// The decks opened for the setup as saved in decks today.txt; an empty record when there are none
decks_on_day saved_decks_today( const wstring &title );

// True when the item has to be ticked before the setup opens: a pick item, or a sequence item
// with no place saved yet.
bool needs_ticks( const setup &chosen, const layout_item &item );

// Works out the windows to open without opening them or creating any file. A notes window's
// path is the name notes_file would choose. A sequence item opens its ticked decks, or else
// decks_to_open. Returns no windows when there is nothing to open.
vector<planned_window> plan_setup( const setup &chosen, const vector<vector<wstring>> &picks, bool separate,
                                   vector<wstring> &problems );

// Creates the notes files, closes old notes tabs, switches to the setup's desktop and opens the plan.
// Returns which window shows which file; problems gets what went wrong.
vector<opened_window> open_plan( const setup &chosen, const vector<planned_window> &plan, vector<wstring> &problems );

// The decks of a sequence item that the opened windows show, as paths in section order.
vector<wstring> decks_shown( const layout_item &item, const vector<opened_window> &opened );

// Saves deck_name as the setup's place in places.txt. False when it could not be saved.
bool save_place( const wstring &title, const wstring &deck_name );

extern const wchar_t *place_problem;    // what to show when save_place fails

// Adds deck_names to the decks opened today for the setup in decks today.txt (see after_opening).
// False when they could not be saved.
bool save_decks_today( const wstring &title, const vector<wstring> &deck_names );

extern const wchar_t *decks_today_problem;  // what to show when save_decks_today fails

// Adds the decks shown for each sequence item to decks today.txt, and saves the highest one
// as the setup's place. Returns decks_today_problem or place_problem when one could not be
// saved, otherwise "".
wstring save_places( const setup &chosen, const vector<opened_window> &opened );

// picks[i] holds the PDFs ticked for setup item i (pick items, and sequence items when not empty).
// separate puts each picked PDF in its own PdfCraft window.
// Saves today's decks and the place of each sequence item that opened.
// Returns problems to show the user; empty when everything opened. When opened is given, it gets
// which window shows which file.
vector<wstring> open_setup( const setup &chosen, const vector<vector<wstring>> &picks, bool separate,
                            vector<opened_window> *opened = nullptr );
