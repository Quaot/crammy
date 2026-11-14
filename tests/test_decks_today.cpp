// Written from tests\specs\decks_today.md.

#include "check.h"
#include "../src/sequence.h"

namespace {

const wstring today = L"2026-11-16";

const wstring arrays = L"3.01 Arrays.pdf";
const wstring linear = L"3.02 Linear search.pdf";
const wstring binary = L"3.03 Binary search.pdf";
const wstring sorting = L"3.04 Sorting.pdf";

// The list that deck_order gives
vector<wstring> ordered_decks() {
    return { arrays, linear, binary, sorting };
}

decks_on_day make_record( const wstring &date, const vector<wstring> &files ) {
    decks_on_day record;
    record.date = date;
    record.files = files;
    return record;
}

bool same_record( const decks_on_day &a, const decks_on_day &b ) {
    return a.date == b.date && a.files == b.files;
}

bool same_map( const std::map<wstring, decks_on_day> &a, const std::map<wstring, decks_on_day> &b ) {
    if ( a.size() != b.size() ) return false;
    for ( const auto &entry : a ) {
        auto found = b.find( entry.first );
        if ( found == b.end() || !same_record( entry.second, found->second ) ) return false;
    }
    return true;
}

void check_reading() {
    auto one = read_decks_today( L"ECE 150: Lecture = 2026-11-16 | 3.02 Linear search.pdf ; 3.03 Binary search.pdf" );
    check( one.size() == 1 && one.count( L"ECE 150: Lecture" ) == 1, "one line reads as one record under its title" );
    check( one[L"ECE 150: Lecture"].date == L"2026-11-16", "the record has the date from the line" );
    check( one[L"ECE 150: Lecture"].files == vector<wstring>( { linear, binary } ),
           "the record has the file names in the order written" );

    auto spaced = read_decks_today( L"  ECE 150: Lecture   =   2026-11-16   |   3.02 Linear search.pdf   ;   3.03 Binary search.pdf  " );
    check( spaced.size() == 1 && spaced.count( L"ECE 150: Lecture" ) == 1, "spaces around the title are trimmed" );
    check( spaced[L"ECE 150: Lecture"].date == L"2026-11-16", "spaces around the date are trimmed" );
    check( spaced[L"ECE 150: Lecture"].files == vector<wstring>( { linear, binary } ),
           "spaces around each file name are trimmed" );

    auto lf = read_decks_today( L"A = 2026-11-16 | 3.01 Arrays.pdf\nB = 2026-11-13 | 3.02 Linear search.pdf\n" );
    check( lf.size() == 2 && lf[L"A"].files == vector<wstring>( { arrays } ) && lf[L"B"].date == L"2026-11-13",
           "lines ending in a line feed both read" );

    auto crlf = read_decks_today( L"A = 2026-11-16 | 3.01 Arrays.pdf\r\nB = 2026-11-13 | 3.02 Linear search.pdf\r\n" );
    check( crlf.size() == 2 && crlf[L"A"].files == vector<wstring>( { arrays } ) && crlf[L"B"].date == L"2026-11-13",
           "lines ending in a carriage return and line feed both read" );
    check( crlf[L"B"].files == vector<wstring>( { linear } ), "a carriage return does not stay on the last file name" );

    auto messy = read_decks_today( L"A = 2026-11-16 | 3.01 Arrays.pdf\r\n"
                                   L"\r\n"
                                   L"no equals sign | 3.02 Linear search.pdf\r\n"
                                   L"C = 2026-11-16 without a bar\r\n"
                                   L"D = 2026-11-13 | 3.04 Sorting.pdf\r\n" );
    check( messy.size() == 2 && messy.count( L"A" ) == 1 && messy.count( L"D" ) == 1,
           "blank lines and lines missing ' = ' or ' | ' are ignored" );
    check( messy[L"A"].files == vector<wstring>( { arrays } ) && messy[L"D"].files == vector<wstring>( { sorting } ),
           "the other lines still read" );

    auto equals = read_decks_today( L"Rate = fast = 2026-11-16 | 3.01 Arrays.pdf" );
    check( equals.size() == 1 && equals.count( L"Rate" ) == 1, "only the first equals sign separates the title" );
    check( equals[L"Rate"].date == L"fast = 2026-11-16", "the rest after the first equals sign may hold equals signs" );

    check( read_decks_today( L"" ).empty(), "empty text reads as an empty map" );
}

void check_writing() {
    check( write_decks_today( {} ) == L"", "an empty map writes an empty text" );

    std::map<wstring, decks_on_day> one;
    one[L"ECE 150: Lecture"] = make_record( L"2026-11-16", { linear, binary } );
    check( write_decks_today( one ) == L"ECE 150: Lecture = 2026-11-16 | 3.02 Linear search.pdf ; 3.03 Binary search.pdf\r\n",
           "a record writes as one line ending in a carriage return and line feed" );

    std::map<wstring, decks_on_day> two;
    two[L"B setup"] = make_record( L"2026-11-13", { sorting } );
    two[L"A setup"] = make_record( L"2026-11-16", { arrays, linear } );
    check( write_decks_today( two ) ==
               L"A setup = 2026-11-16 | 3.01 Arrays.pdf ; 3.02 Linear search.pdf\r\n"
               L"B setup = 2026-11-13 | 3.04 Sorting.pdf\r\n",
           "records are written one line each in the map's order" );
    check( same_map( read_decks_today( write_decks_today( two ) ), two ), "reading what was written gives back the same map" );
}

void check_path() {
    wstring path = decks_today_path();
    wstring ending = L"\\decks today.txt";
    check( path.size() >= ending.size() && path.compare( path.size() - ending.size(), ending.size(), ending ) == 0,
           "the decks today path ends with \\decks today.txt" );
}

void check_decks_to_open() {
    vector<wstring> ordered = ordered_decks();

    check( decks_to_open( ordered, binary, make_record( today, { linear, binary } ), today ) == vector<wstring>( { linear, binary } ),
           "decks already opened today open again, in order, and the place is not moved on" );
    check( decks_to_open( ordered, arrays, make_record( L"2026-11-13", { arrays } ), today ) == vector<wstring>( { linear } ),
           "a record from another day is ignored and the deck after the place opens" );
    check( decks_to_open( ordered, linear, decks_on_day(), today ) == vector<wstring>( { binary } ),
           "with an empty record the deck after the place opens" );
    check( decks_to_open( ordered, L"", decks_on_day(), today ).empty(), "with an empty record and no place nothing opens" );
    check( decks_to_open( ordered, linear, make_record( today, { L"9.99 Gone.pdf" } ), today ) == vector<wstring>( { binary } ),
           "when every deck from today is gone the deck after the place opens" );
    check( decks_to_open( ordered, linear, make_record( today, { linear, L"9.99 Gone.pdf" } ), today ) == vector<wstring>( { linear } ),
           "decks from today that are gone from the list are left out" );
}

void check_after_opening() {
    decks_on_day empty;
    decks_on_day first = after_opening( empty, today, { linear } );
    check( first.date == today && first.files == vector<wstring>( { linear } ),
           "opening into an empty record starts it with today's date and the files" );

    decks_on_day second = after_opening( first, today, { binary } );
    check( second.date == today && second.files == vector<wstring>( { linear, binary } ),
           "opening on the same day adds the new file at the end" );

    decks_on_day third = after_opening( second, today, { L"3.02 linear search.PDF", sorting } );
    check( third.files == vector<wstring>( { linear, binary, sorting } ),
           "a file already there is left out whatever its case" );

    decks_on_day fresh = after_opening( make_record( L"2026-11-13", { arrays } ), today, { linear } );
    check( fresh.date == today && fresh.files == vector<wstring>( { linear } ),
           "opening on another day starts the record again" );

    check( same_record( after_opening( second, today, {} ), second ), "opening no files leaves the record unchanged" );
    decks_on_day old_record = make_record( L"2026-11-13", { arrays } );
    check( same_record( after_opening( old_record, today, {} ), old_record ),
           "opening no files leaves an old record unchanged too" );
    check( same_record( after_opening( empty, today, {} ), empty ), "opening no files leaves an empty record unchanged" );
}

}  // namespace

void test_decks_today() {
    check_reading();
    check_writing();
    check_path();
    check_decks_to_open();
    check_after_opening();
}
