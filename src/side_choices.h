#pragma once

#include "profiles.h"

// What can be put on an empty part of the screen picture, and how the choice becomes a window
// line. Nothing here shows a window, so the tests can call all of it.

enum class side_choice { pdf, newest, sequence, pick, folder, web, notes };

// What the menu and the dialogs after it gave back. paths are full paths of the PDFs or the one
// folder chosen, or the web address as typed; notes needs none.
struct side_answer {
    bool accepted = false;
    side_choice choice = side_choice::pdf;
    vector<wstring> paths;
};

// The window line for an answer, with paths inside root stored relative to it. False, with a
// sentence in problem, when the answer cannot make a line.
bool item_for_answer( const side_answer &answer, const wstring &side, const wstring &root, written_item &item,
                      wstring &problem );

// The folder the file and folder dialogs start in: root when it is a folder, else Downloads.
wstring start_folder_for( const wstring &root );

// Which side a click at a fraction across the screen picture fills, or "" when that part
// already has a window.
wstring empty_side_at( const vector<written_item> &items, double across, int split );

// A web address from what was typed or dropped: "https://" is added when there is no
// http:// or https:// in front. "" when the text is empty or has a space inside.
wstring web_address( const wstring &text );

// The short name shown on a web tile: the host without "www.", such as "learn.uwaterloo.ca".
wstring web_label( const wstring &address );

// The address in the text of an Internet shortcut (.url file), from its URL= line, or "".
wstring address_in_shortcut( const wstring &text );

// The splits offered in the main window, as the left side's share in percent with its label.
struct split_choice {
    int split;
    wstring label;      // "50 / 50", "60 / 40" and so on
};

// 50, 60, 75, 40 and 25 in that order, then current when it is none of these.
vector<split_choice> split_choices( int current );

// Gives the setup (or, with setup_index -1, the subject) this split. A setup whose split matches
// its subject's is left without its own, and a subject at 50 is left without one.
void set_split( profile_book &book, size_t profile_index, int setup_index, int split );
