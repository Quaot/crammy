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

// The decks opened for one setup on one day
struct decks_on_day {
    wstring date;               // "2026-11-16"
    vector<wstring> files;      // deck file names in the order they were opened, no repeats
};

// One record per line: the setup title, " = ", the date, " | ", then the file names separated by " ; ".
std::map<wstring, decks_on_day> read_decks_today( const wstring &text );
wstring write_decks_today( const std::map<wstring, decks_on_day> &records );
wstring decks_today_path();     // "decks today.txt" next to crammy.exe

// The decks to open for a setup: the ones already opened today when there are any,
// otherwise the deck after the place (one deck), otherwise none.
vector<wstring> decks_to_open( const vector<wstring> &ordered, const wstring &place,
                               const decks_on_day &record, const wstring &today );

// The record after opening files today: on the same day the new files are added at the end,
// leaving out ones already there; on another day (or an empty record) it starts again.
decks_on_day after_opening( const decks_on_day &record, const wstring &today, const vector<wstring> &files );
