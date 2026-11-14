#pragma once

#include "files.h"
#include "tiling.h"

// The PDFs a pick item offers to tick. A sequence item offers its numbered decks in section order.
vector<pdf_file> pick_files( const layout_item &item );

// True when the item has to be ticked before the setup opens: a pick item, or a sequence item
// with no place saved yet.
bool needs_ticks( const setup &chosen, const layout_item &item );

// Works out the windows to open without opening them or creating any file. A notes window's
// path is the name notes_file would choose. Returns no windows when there is nothing to open.
vector<planned_window> plan_setup( const setup &chosen, const vector<vector<wstring>> &picks, bool separate,
                                   vector<wstring> &problems );

// Creates the notes files, closes old notes tabs, switches to the setup's desktop and opens the plan.
// Returns which window shows which file; problems gets what went wrong.
vector<opened_window> open_plan( const setup &chosen, const vector<planned_window> &plan, vector<wstring> &problems );

// picks[i] holds the PDFs ticked for setup item i (only pick items use it).
// separate puts each picked PDF in its own PdfCraft window.
// Returns problems to show the user; empty when everything opened.
vector<wstring> open_setup( const setup &chosen, const vector<vector<wstring>> &picks, bool separate );
