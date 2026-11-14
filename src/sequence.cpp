#include "sequence.h"

#include <algorithm>
#include <windows.h>

#include "text.h"

namespace {

// One dot-separated piece of a section number: 18, 18a or A.
struct section_part {
    bool is_number = false;
    unsigned long long number = 0;
    wstring letters;                // lower case; what follows the number, or the whole part
};

bool is_digit( wchar_t letter ) {
    return letter >= L'0' && letter <= L'9';
}

section_part read_part( const wstring &text ) {
    section_part part;
    size_t digits = 0;
    while ( digits < text.size() && is_digit( text[digits] ) ) ++digits;
    part.is_number = digits > 0;
    if ( part.is_number ) part.number = std::stoull( text.substr( 0, digits ) );
    part.letters = lower( text.substr( digits ) );
    return part;
}

// The parts of the section number at the start of a file name; none when it does not start with a digit.
vector<section_part> section_of( const wstring &file_name ) {
    vector<section_part> parts;
    if ( file_name.empty() || !is_digit( file_name[0] ) ) return parts;

    size_t end = file_name.find( L' ' );
    wstring number = file_name.substr( 0, end );
    if ( end == wstring::npos && ends_with( lower( number ), L".pdf" ) ) number.resize( number.size() - 4 );

    size_t start = 0;
    while ( true ) {
        size_t dot = number.find( L'.', start );
        parts.push_back( read_part( number.substr( start, dot == wstring::npos ? wstring::npos : dot - start ) ) );
        if ( dot == wstring::npos ) break;
        start = dot + 1;
    }
    return parts;
}

// Negative when a comes first, positive when b does, zero when they are the same part.
int compare_parts( const section_part &a, const section_part &b ) {
    if ( a.is_number != b.is_number ) return a.is_number ? -1 : 1;
    if ( a.is_number && a.number != b.number ) return a.number < b.number ? -1 : 1;
    return a.letters.compare( b.letters );
}

}  // namespace

bool deck_before( const wstring &a, const wstring &b ) {
    vector<section_part> first = section_of( a ), second = section_of( b );
    for ( size_t i = 0; i < first.size() && i < second.size(); ++i ) {
        int order = compare_parts( first[i], second[i] );
        if ( order != 0 ) return order < 0;
    }
    return first.size() < second.size();
}

vector<wstring> deck_order( const vector<wstring> &file_names ) {
    vector<wstring> decks;
    for ( const wstring &name : file_names ) if ( !section_of( name ).empty() ) decks.push_back( name );
    std::stable_sort( decks.begin(), decks.end(), deck_before );
    return decks;
}

wstring deck_after( const vector<wstring> &ordered, const wstring &place ) {
    if ( ordered.empty() ) return L"";
    size_t next = 0;
    while ( next < ordered.size() && deck_before( ordered[next], place ) ) ++next;
    if ( next < ordered.size() && ordered[next] == place ) ++next;
    return ordered[ std::min( next, ordered.size() - 1 ) ];
}

wstring deck_label( const wstring &file_name ) {
    if ( ends_with( lower( file_name ), L".pdf" ) ) return file_name.substr( 0, file_name.size() - 4 );
    return file_name;
}

std::map<wstring, wstring> read_places( const wstring &text ) {
    std::map<wstring, wstring> places;
    size_t start = 0;
    while ( start <= text.size() ) {
        size_t end = text.find( L'\n', start );
        wstring line = text.substr( start, end == wstring::npos ? wstring::npos : end - start );
        start = end == wstring::npos ? text.size() + 1 : end + 1;

        size_t equals = line.find( L'=' );
        if ( equals == wstring::npos ) continue;
        wstring title = trim( line.substr( 0, equals ) ), file = trim( line.substr( equals + 1 ) );
        if ( !title.empty() && !file.empty() ) places[title] = file;
    }
    return places;
}

wstring write_places( const std::map<wstring, wstring> &places ) {
    wstring text;
    for ( const auto &[title, file] : places ) text += title + L" = " + file + L"\r\n";
    return text;
}

wstring places_path() {
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW( nullptr, exe_path, MAX_PATH );
    return join_path( parent_folder( exe_path ), L"places.txt" );
}
