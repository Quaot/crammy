#pragma once

#include <map>
#include <string>
#include <vector>

using std::wstring;
using std::vector;

// The file names in section order. Names that do not start with a section number are left out.
vector<wstring> deck_order( const vector<wstring> &file_names );

// True when deck a comes before deck b. Both must start with a section number.
bool deck_before( const wstring &a, const wstring &b );

// The deck after place in ordered (a list from deck_order). The last deck stays the last.
// A place that is not in the list counts as the spot where it would sort.
wstring deck_after( const vector<wstring> &ordered, const wstring &place );

// The file name without ".pdf", for showing.
wstring deck_label( const wstring &file_name );

// One place per line: the setup title, " = ", then the file name of the deck reached.
std::map<wstring, wstring> read_places( const wstring &text );
wstring write_places( const std::map<wstring, wstring> &places );
wstring places_path();      // places.txt next to crammy.exe
