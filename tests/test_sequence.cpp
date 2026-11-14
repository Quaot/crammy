// Written from tests\specs\sequence.md.

#include "check.h"
#include "../src/sequence.h"
#include "../src/profiles.h"

namespace {

// The 21 lecture decks in the order they should come out
vector<wstring> expected_order() {
    return {
        L"0.Welcome.pdf",
        L"1.01 Hello world!.pdf",
        L"1.02 Anatomy of a program.pdf",
        L"1.10 Assignment operators.pdf",
        L"1.18 Writing functions.pdf",
        L"1.18a Function tutorial.pdf",
        L"1.18b Compiler errors.pdf",
        L"1.19 Assertions.pdf",
        L"1.27 Bit-wise and -shift operators.pdf",
        L"1.A Ternary operator.pdf",
        L"2 Dynamic memory allocation.pdf",
        L"2.01 Pointers.pdf",
        L"2.05 Memory.pdf",
        L"2.05.1 Wild pointers.pdf",
        L"2.05.2 Dangling pointers.pdf",
        L"2.06 Arrays.pdf",
        L"3.08 Searching.pdf",
        L"3.9 Rotating an array to the left using reverse.pdf",
        L"3.10 Sorting.pdf",
        L"7.04 Insertion sort.pdf",
        L"7.5 Counting sort.pdf",
    };
}

// The names that do not start with a section number
vector<wstring> unnumbered_names() {
    return { L"A polynomial class.pdf", L"Date class.pdf", L"Using replit.pdf", L"assignment1.pdf" };
}

// Every name, with the unnumbered ones mixed in and the order scrambled
vector<wstring> scrambled_names() {
    vector<wstring> names = expected_order();
    for ( const wstring &name : unnumbered_names() ) names.push_back( name );
    vector<wstring> scrambled;
    for ( size_t at = 0; at < names.size(); at += 2 ) scrambled.push_back( names[at] );
    for ( size_t at = 1; at < names.size(); at += 2 ) scrambled.insert( scrambled.begin(), names[at] );
    return scrambled;
}

void check_deck_order() {
    vector<wstring> want = expected_order();
    check( deck_order( want ) == want, "decks already in order stay in order" );

    vector<wstring> reversed( want.rbegin(), want.rend() );
    check( deck_order( reversed ) == want, "decks given backwards come out in section order" );

    vector<wstring> mixed = scrambled_names();
    check( mixed.size() == 25, "the scrambled list holds all 25 names" );
    vector<wstring> got = deck_order( mixed );
    check( got == want, "a scrambled list with unnumbered names gives the 21 numbered decks in order" );
    check( got.size() == 21, "the four names without a section number are left out" );

    check( deck_order( {} ).empty(), "no names give no decks" );
    check( deck_order( unnumbered_names() ).empty(), "only unnumbered names give no decks" );

    check( deck_order( { L"9 Nine.pdf", L"10 Ten.pdf", L"08 Eight.pdf" } ) ==
               vector<wstring>( { L"08 Eight.pdf", L"9 Nine.pdf", L"10 Ten.pdf" } ),
           "9 is below 10 and 08 is below 9 as whole numbers" );
    check( deck_order( { L"1.18b B.pdf", L"1.18a A.pdf", L"1.18 Plain.pdf" } ) ==
               vector<wstring>( { L"1.18 Plain.pdf", L"1.18a A.pdf", L"1.18b B.pdf" } ),
           "a part with no letter comes before the same number with a letter, and letters go alphabetically" );
    check( deck_order( { L"1.a Lower.pdf", L"1.18 Number.pdf" } ) ==
               vector<wstring>( { L"1.18 Number.pdf", L"1.a Lower.pdf" } ),
           "a number part comes before a letter part, whatever the letter's case" );
    check( deck_order( { L"1.B Second.pdf", L"1.a First.pdf" } ) ==
               vector<wstring>( { L"1.a First.pdf", L"1.B Second.pdf" } ),
           "letter parts compare without regard to case" );
    check( deck_order( { L"2.05.1 Deep.pdf", L"2.05 Shallow.pdf", L"2 Top.pdf" } ) ==
               vector<wstring>( { L"2 Top.pdf", L"2.05 Shallow.pdf", L"2.05.1 Deep.pdf" } ),
           "when every part so far is equal the shorter number comes first" );
    check( deck_order( { L"1.Intro.pdf", L"0.Welcome.pdf" } ) ==
               vector<wstring>( { L"0.Welcome.pdf", L"1.Intro.pdf" } ),
           "a name with no space ends its number at .pdf and a word part counts as a letter part" );
}

void check_deck_before() {
    vector<wstring> want = expected_order();
    bool neighbours_agree = true;
    for ( size_t at = 0; at + 1 < want.size(); ++at ) {
        if ( !deck_before( want[at], want[at + 1] ) ) neighbours_agree = false;
        if ( deck_before( want[at + 1], want[at] ) ) neighbours_agree = false;
    }
    check( neighbours_agree, "deck_before agrees with the order for every pair of neighbours" );

    bool self_false = true;
    for ( const wstring &name : want )
        if ( deck_before( name, name ) ) self_false = false;
    check( self_false, "a deck is not before itself" );

    check( deck_before( L"3.9 Rotating an array to the left using reverse.pdf", L"3.10 Sorting.pdf" ),
           "3.9 comes before 3.10" );
    check( !deck_before( L"3.10 Sorting.pdf", L"3.9 Rotating an array to the left using reverse.pdf" ),
           "3.10 does not come before 3.9" );
    check( deck_before( L"0.Welcome.pdf", L"7.5 Counting sort.pdf" ), "the first deck is before the last" );
    check( !deck_before( L"7.5 Counting sort.pdf", L"0.Welcome.pdf" ), "the last deck is not before the first" );
}

void check_deck_after() {
    vector<wstring> ordered = expected_order();

    check( deck_after( ordered, L"1.18 Writing functions.pdf" ) == L"1.18a Function tutorial.pdf",
           "after 1.18 comes 1.18a" );
    check( deck_after( ordered, L"1.27 Bit-wise and -shift operators.pdf" ) == L"1.A Ternary operator.pdf",
           "after 1.27 comes 1.A" );
    check( deck_after( ordered, L"2.05.2 Dangling pointers.pdf" ) == L"2.06 Arrays.pdf",
           "after 2.05.2 comes 2.06" );
    check( deck_after( ordered, L"7.5 Counting sort.pdf" ) == L"7.5 Counting sort.pdf",
           "after the last deck the last deck comes back" );
    check( deck_after( ordered, L"3.08a Gone.pdf" ) == L"3.9 Rotating an array to the left using reverse.pdf",
           "a place not in the list gives the deck after the highest name that sorts before it" );
    check( deck_after( ordered, L"0 Start.pdf" ) == L"0.Welcome.pdf",
           "a place that sorts before every name gives the first name" );
    check( deck_after( {}, L"1.01 Hello world!.pdf" ).empty(), "an empty list gives an empty name" );
    check( deck_after( {}, L"" ).empty(), "an empty list and an empty place give an empty name" );
}

void check_deck_label() {
    check( deck_label( L"3.02 Linear search.pdf" ) == L"3.02 Linear search", "the label drops .pdf" );
    check( deck_label( L"3.02 Linear search" ) == L"3.02 Linear search", "a name without .pdf comes back as it is" );
    check( deck_label( L"X.PDF" ) == L"X", "the ending is matched without regard to case" );
}

void check_places() {
    wstring text = L"ECE 150: Lecture = 3.02 Linear search.pdf\r\n"
                   L"\r\n"
                   L"this line has no separator\r\n"
                   L"Math 117: Lecture=MATH117-L23.pdf\r\n";
    std::map<wstring, wstring> places = read_places( text );
    check( places.size() == 2, "two places are read and the blank and separator-less lines are ignored" );
    check( places[L"ECE 150: Lecture"] == L"3.02 Linear search.pdf", "the first place is read with its spaces trimmed" );
    check( places[L"Math 117: Lecture"] == L"MATH117-L23.pdf", "the second place is read without spaces around =" );

    std::map<wstring, wstring> with_equals = read_places( L"Title = file=name.pdf\r\n" );
    check( with_equals.size() == 1 && with_equals[L"Title"] == L"file=name.pdf",
           "only the first = separates, so a file name may hold =" );

    std::map<wstring, wstring> plain_newlines = read_places( L"A = a.pdf\nB = b.pdf\n" );
    check( plain_newlines.size() == 2 && plain_newlines[L"A"] == L"a.pdf" && plain_newlines[L"B"] == L"b.pdf",
           "lines ending in a bare newline are read too" );

    check( read_places( L"" ).empty(), "empty text gives no places" );

    wstring written = write_places( places );
    check( written == L"ECE 150: Lecture = 3.02 Linear search.pdf\r\nMath 117: Lecture = MATH117-L23.pdf\r\n",
           "write_places writes title = file lines ending in CRLF in the map's order" );
    check( read_places( written ) == places, "writing then reading places gives the same map" );
    check( write_places( {} ).empty(), "no places write no text" );

    wstring path = places_path();
    wstring ending = L"\\places.txt";
    check( path.size() >= ending.size() && path.compare( path.size() - ending.size(), ending.size(), ending ) == 0,
           "places_path ends with \\places.txt" );
}

bool starts_with_line( const wstring &error, int number ) {
    wstring want = L"line " + std::to_wstring( number );
    return error.compare( 0, want.size(), want ) == 0;
}

void check_profiles_lines() {
    wstring text = L"[X]\r\n"
                   L"root = Downloads\\X\r\n"
                   L"[X: Lecture]\r\n"
                   L"full = sequence \"1 Lectures\\PDFs\"\r\n";
    profile_book book = read_book( text );
    check( book.errors.empty(), "a sequence line with one path reads without errors" );
    bool shaped = book.profiles.size() == 1 && book.profiles[0].setups.size() == 1 &&
                  book.profiles[0].setups[0].items.size() == 1;
    check( shaped, "the setup holds one item" );
    if ( shaped ) {
        const written_item &item = book.profiles[0].setups[0].items[0];
        check( item.kind == item_kind::sequence, "the item has kind sequence" );
        check( item.side == L"full", "the item has side full" );
        check( item.paths.size() == 1 && item.paths[0] == L"1 Lectures\\PDFs", "the item has the single path" );

        wstring written = write_book( book );
        check( written.find( L"full = sequence \"1 Lectures\\PDFs\"" ) != wstring::npos,
               "write_book writes the sequence line" );
        profile_book again = read_book( written );
        bool same = again.errors.empty() && again.profiles.size() == 1 && again.profiles[0].setups.size() == 1 &&
                    again.profiles[0].setups[0].items.size() == 1;
        check( same, "reading the written text again gives one setup with one item" );
        if ( same ) {
            const written_item &back = again.profiles[0].setups[0].items[0];
            check( back.kind == item_kind::sequence && back.side == L"full" && back.paths == item.paths,
                   "the item read back is the same" );
        }
    }

    profile_book two = read_book( L"[X]\r\nroot = Downloads\\X\r\n[X: Lecture]\r\nfull = sequence \"a\" \"b\"\r\n" );
    check( !two.errors.empty() && starts_with_line( two.errors[0], 4 ),
           "a sequence line with two paths gives an error that starts with line 4" );

    profile_book none = read_book( L"[X]\r\nroot = Downloads\\X\r\n[X: Lecture]\r\nfull = sequence\r\n" );
    check( !none.errors.empty() && starts_with_line( none.errors[0], 4 ),
           "a sequence line with no path gives an error that starts with line 4" );
}

}  // namespace

void test_sequence() {
    check_deck_order();
    check_deck_before();
    check_deck_after();
    check_deck_label();
    check_places();
    check_profiles_lines();
}
