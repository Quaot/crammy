#include "slide_watch.h"

#include "text.h"

namespace {

// A download is left alone until its size has stayed the same this long.
const unsigned long long settle_ms = 2000;

// True when name fits pattern, where * is any run of characters and ? is one character.
// Both are already lower case.
bool name_fits( const wstring &name, const wstring &pattern ) {
    size_t n = 0;                   // next character of name
    size_t p = 0;                   // next character of pattern
    size_t star = wstring::npos;    // where the last * sits in the pattern
    size_t star_n = 0;              // where in name that * was last tried

    while ( n < name.size() ) {
        if ( p < pattern.size() && ( pattern[p] == L'?' || pattern[p] == name[n] ) ) {
            n++;
            p++;
        } else if ( p < pattern.size() && pattern[p] == L'*' ) {
            star = p;
            star_n = n;
            p++;
        } else if ( star != wstring::npos ) {
            // Let the last * take one more character and try again
            star_n++;
            n = star_n;
            p = star + 1;
        } else {
            return false;
        }
    }
    while ( p < pattern.size() && pattern[p] == L'*' ) p++;
    return p == pattern.size();
}

}  // namespace

bool matches_pattern( const wstring &path, const wstring &pattern ) {
    if ( lower( parent_folder( path ) ) != lower( parent_folder( pattern ) ) ) return false;
    return name_fits( lower( file_name( path ) ), lower( file_name( pattern ) ) );
}

wstring replacement_for( const watched_slide &watch, const vector<seen_file> &files ) {
    wstring best_path;
    unsigned long long best_modified = watch.open_modified;

    for ( const seen_file &file : files ) {
        if ( !matches_pattern( file.path, watch.pattern ) ) continue;
        if ( lower( file.path ) == lower( watch.open_path ) ) continue;
        if ( file.modified <= best_modified ) continue;     // not newer than the open file or the best so far
        if ( file.size_changed_ms_ago < settle_ms ) continue;   // still being written
        best_path = file.path;
        best_modified = file.modified;
    }
    return best_path;
}
