#pragma once

#include <windows.h>

#include "profiles.h"

struct name_answer {
    bool accepted = false;
    wstring subject;
    wstring name;
};

// Asks for a subject and a setup name. subjects fills the subject list; it can be typed over.
name_answer ask_for_name( HWND owner, const wchar_t *title, const vector<wstring> &subjects, const wstring &subject,
                          const wstring &name, bool subject_fixed );

struct pick_answer {
    bool accepted = false;
    vector<vector<wstring>> picks;  // picks[i] holds the PDFs ticked for setup item i
    bool separate = false;
};

// Lets the user tick which PDFs the setup's pick windows should open.
pick_answer ask_for_picks( HWND owner, const setup &chosen );

// Asks for a web address. "" when cancelled; otherwise the text as typed.
wstring ask_for_address( HWND owner );

// The Windows file dialog for one or more PDFs, starting in start_folder. Empty when cancelled.
vector<wstring> ask_for_pdfs( HWND owner, const wstring &start_folder );

// The Windows folder dialog, starting in start_folder. "" when cancelled.
wstring ask_for_folder( HWND owner, const wstring &start_folder, const wchar_t *title );
