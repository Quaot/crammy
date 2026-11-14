#pragma once

#include <string>
#include <vector>

using std::wstring;
using std::vector;

struct watched_slide {
    wstring pattern;                    // a full path whose file name may hold * and ?, as newest lines use
    wstring open_path;                  // the file now open
    unsigned long long open_modified;   // its modified time when it was opened
};

struct seen_file {
    wstring path;
    unsigned long long modified;
    unsigned long long size;
    unsigned long long size_changed_ms_ago;    // how long since its size last changed
};

// The path to open in place of the open file, or "" when nothing should change.
wstring replacement_for( const watched_slide &watch, const vector<seen_file> &files );

// True when the file name of path matches the file name in pattern, where * is any run of
// characters and ? is one character, ignoring case. The folders must be equal, ignoring case.
bool matches_pattern( const wstring &path, const wstring &pattern );
