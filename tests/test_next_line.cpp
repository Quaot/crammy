// Written from tests\specs\next_line.md.

#include <windows.h>

#include <algorithm>

#include "check.h"
#include "../src/next_line.h"

namespace {

wstring find_timetable_file() {
    wchar_t buffer[MAX_PATH * 4] = {};
    GetModuleFileNameW( nullptr, buffer, MAX_PATH * 4 );
    wstring folder = parent_folder( buffer );
    wstring beside = join_path( folder, L"timetable.txt" );
    if ( is_file( beside ) ) {
        return beside;
    }
    return join_path( join_path( folder, L".." ), L"timetable.txt" );
}

vector<class_block> real_blocks() {
    vector<wstring> errors;
    return read_timetable( read_text_file( find_timetable_file() ), errors );
}

vector<wstring> known_courses() {
    return { L"ECE 105", L"ECE 150", L"ECE 190", L"ECE 198", L"ENGL 192", L"Math 115", L"Math 117" };
}

const wstring dot = L" \u00B7 ";
const wstring nl = L"\n";
const wstring ellipsis = L"\u2026";

bool is_class( const next_class &next, const wstring &course, const wstring &kind, int start_minute,
               const wstring &room ) {
    return next.found && next.meeting.course == course && next.meeting.kind == kind
           && next.meeting.start_minute == start_minute && next.meeting.room == room;
}

// A class made by hand for the line text checks
next_class made( bool today, calendar_date date, const wstring &course, const wstring &kind, int start_minute,
                 int end_minute, const wstring &room ) {
    next_class next;
    next.found = true;
    next.today = today;
    next.date = date;
    next.meeting.course = course;
    next.meeting.kind = kind;
    next.meeting.room = room;
    next.meeting.start_minute = start_minute;
    next.meeting.end_minute = end_minute;
    return next;
}

void check_finding() {
    vector<class_block> blocks = real_blocks();
    vector<wstring> known = known_courses();
    calendar_date monday = { 2026, 11, 16 };

    next_class next = find_next_class( blocks, monday, 400, known );
    check( is_class( next, L"MATH 117", L"TUT", 510, L"DWE 1515" ) && next.today && next.meeting.end_minute == 620,
           "at 06:40 on Monday the class is the MATH 117 TUT, today" );
    check( next.date == monday, "the class found today carries today's date" );

    next = find_next_class( blocks, monday, 790, known );
    check( is_class( next, L"MATH 117", L"LEC", 750, L"PSE 5353" ) && next.today && next.meeting.end_minute == 800,
           "at 13:10 the MATH 117 LEC is on now, so it is the class" );

    next = find_next_class( blocks, monday, 805, known );
    check( is_class( next, L"ECE 150", L"LEC", 810, L"PSE 5353" ) && next.today,
           "at 13:25 the class is the ECE 150 LEC, today" );
    check( next.known, "ECE 150 is a known course" );

    next = find_next_class( blocks, monday, 825, known );
    check( is_class( next, L"ECE 150", L"LEC", 810, L"PSE 5353" ) && next.today,
           "at 13:45 the ECE 150 LEC is on now and is still the class" );

    next = find_next_class( blocks, monday, 1320, known );
    check( is_class( next, L"MATH 115", L"TUT", 630, L"DWE 3516" ) && !next.today && next.meeting.end_minute == 740,
           "at 22:00 the class is the MATH 115 TUT on Tuesday, not today" );
    check( next.date == calendar_date { 2026, 11, 17 }, "the next class is dated Tuesday 2026-11-17" );
    check( next.known, "MATH 115 is known although the list writes it Math 115" );

    // The last class of Monday has ended, so the search moves to Tuesday
    next = find_next_class( blocks, monday, 980, known );
    check( next.found && !next.today && next.date == calendar_date { 2026, 11, 17 },
           "at 16:20 the last class of Monday has ended, so the class is on Tuesday" );

    // GENE 119 is not in the known list
    next = find_next_class( blocks, monday, 625, known );
    check( is_class( next, L"GENE 119", L"SEM", 630, L"STC 0050" ) && next.today,
           "at 10:25 on Monday the MATH 117 TUT has ended and the class is the GENE 119 SEM" );
    check( !next.known, "GENE 119 is not a known course" );

    next = find_next_class( blocks, { 2026, 11, 21 }, 600, known );
    check( is_class( next, L"MATH 117", L"TUT", 510, L"DWE 1515" ) && !next.today,
           "on Saturday the class is the MATH 117 TUT on Monday at 08:30" );
    check( next.date == calendar_date { 2026, 11, 23 }, "the Saturday search finds Monday 2026-11-23" );

    // One block 21 days away is out of reach
    vector<wstring> errors;
    vector<class_block> far_away =
        read_timetable( L"ECE 150 | LEC | M | 13:30-14:20 | PSE 5353 | 2026-12-28 to 2026-12-28", errors );
    check( errors.empty() && far_away.size() == 1, "the single far block reads without errors" );
    next = find_next_class( far_away, { 2026, 12, 7 }, 600, known );
    check( !next.found, "a class 21 days on is not found" );

    next = find_next_class( {}, monday, 600, known );
    check( !next.found, "an empty list gives no class" );
}

void check_line_today() {
    calendar_date monday = { 2026, 11, 16 };
    next_class later = made( true, monday, L"ECE 150", L"LEC", 810, 860, L"PSE 5353" );
    const wstring top = L"ECE 150 lec" + nl;

    check( line_text( later, 805, L"" ) == top + L"PSE 5353" + dot + L"in 5m",
           "a class starting in 5 minutes reads in 5m on the bottom row" );
    check( line_text( later, 750, L"" ) == top + L"PSE 5353" + dot + L"in 60m",
           "a class exactly 60 minutes away reads in 60m" );
    check( line_text( later, 749, L"" ) == top + L"PSE 5353" + dot + L"13:30",
           "a class 61 minutes away reads its start time" );
    check( line_text( later, 760, L"" ) == top + L"PSE 5353" + dot + L"in 50m",
           "a class starting in 50 minutes reads in 50m" );
    check( line_text( later, 825, L"" ) == top + L"PSE 5353" + dot + L"now",
           "a class that is on reads now" );
    check( line_text( later, 810, L"" ) == top + L"PSE 5353" + dot + L"now",
           "a class reads now from its start minute" );
    check( line_text( later, 700, L"" ) == top + L"PSE 5353" + dot + L"13:30",
           "a class more than an hour away reads its start time" );

    next_class morning = made( true, monday, L"MATH 117", L"TUT", 510, 620, L"DWE 1515" );
    check( line_text( morning, 400, L"" ) == L"MATH 117 tut" + nl + L"DWE 1515" + dot + L"08:30",
           "the start time has two digits for the hour" );

    check( line_text( later, 825, L"3.02 Linear search" ) == top + L"3.02 Linear search",
           "a deck is the whole bottom row for a class that is on" );
    check( line_text( later, 805, L"3.02 Linear search" ) == top + L"3.02 Linear search",
           "a deck replaces the room and the in-minutes too, and the top row stays" );

    next_class project = made( true, monday, L"ECE 198", L"PRJ", 810, 860, L"E7 4433" );
    check( line_text( project, 825, L"" ) == L"ECE 198 prj" + nl + L"E7 4433" + dot + L"now",
           "the kind PRJ reads prj" );

    wstring two_rows = line_text( later, 805, L"" );
    check( count( two_rows.begin(), two_rows.end(), L'\n' ) == 1 && two_rows.back() != L'\n',
           "the line has one newline between the rows and none at the end" );
}

void check_line_other_day() {
    next_class tuesday = made( false, { 2026, 11, 17 }, L"MATH 115", L"TUT", 630, 740, L"DWE 3516" );
    check( line_text( tuesday, 1320, L"" ) == L"MATH 115 tut" + nl + L"Tue 10:30" + dot + L"DWE 3516",
           "a class on another day reads the course and kind, then day, time and room" );
    check( line_text( tuesday, 1320, L"3.02 Linear search" ) == L"MATH 115 tut" + nl + L"Tue 10:30" + dot + L"DWE 3516",
           "a deck does not change the line of a class on another day" );

    const wchar_t *names[7] = { L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat", L"Sun" };
    for ( int offset = 0; offset < 7; ++offset ) {
        next_class day = made( false, { 2026, 11, 16 + offset }, L"ECE 105", L"LEC", 540, 590, L"PSE 1" );
        check( line_text( day, 100, L"" ) == L"ECE 105 lec" + nl + wstring( names[offset] ) + L" 09:00" + dot + L"PSE 1",
               "the day name is right for each day of the week" );
    }
}

void check_line_special() {
    next_class none;
    none.found = false;
    check( line_text( none, 600, L"" ) == L"No classes", "no class gives No classes" );
    check( line_text( none, 600, L"3.02 Linear search" ) == L"No classes",
           "no class gives No classes even with a deck" );
    check( line_text( none, 600, L"" ).find( L'\n' ) == wstring::npos, "No classes has no newline" );
}

void check_line_length() {
    calendar_date monday = { 2026, 11, 16 };
    next_class on_now = made( true, monday, L"ECE 150", L"LEC", 810, 860, L"PSE 5353" );
    const wstring top = L"ECE 150 lec" + nl;

    wstring long_deck = L"1.08 Conditional statements and comparison operators";
    wstring expected = top + long_deck.substr( 0, 23 ) + ellipsis;
    wstring got = line_text( on_now, 825, long_deck );
    check( got == expected, "a long deck is cut on the bottom row to 23 characters and an ellipsis" );
    check( got.size() == 11 + 1 + 24, "the cut bottom row is 24 characters beside the top row" );
    check( got.back() == ellipsis[0], "a long bottom row ends in the ellipsis character" );

    wstring deck_24 = L"123456789012345678901234";
    check( line_text( on_now, 825, deck_24 ) == top + deck_24, "a bottom row of exactly 24 characters is left alone" );

    wstring deck_25 = deck_24 + L"5";
    check( line_text( on_now, 825, deck_25 ) == top + deck_24.substr( 0, 23 ) + ellipsis,
           "a bottom row of 25 characters is cut" );

    // The two rows together are over 24 characters, but each row is counted on its own
    wstring deck_20 = L"12345678901234567890";
    check( line_text( on_now, 825, deck_20 ) == top + deck_20, "rows are counted one at a time, not together" );

    // A top row over 24 characters is cut too
    next_class long_name = made( true, monday, L"ABCDEFGHIJKLMNOPQRSTUV", L"LEC", 810, 860, L"PSE 5353" );
    check( line_text( long_name, 825, L"" ) == L"ABCDEFGHIJKLMNOPQRSTUV l" + ellipsis + nl + L"PSE 5353" + dot + L"now",
           "a top row over 24 characters is cut on its own" );
}

void check_line_real() {
    vector<class_block> blocks = real_blocks();
    vector<wstring> known = known_courses();
    calendar_date monday = { 2026, 11, 16 };

    next_class next = find_next_class( blocks, monday, 805, known );
    check( line_text( next, 805, L"" ) == L"ECE 150 lec" + nl + L"PSE 5353" + dot + L"in 5m",
           "the real timetable at 13:25 gives the in 5m line" );
    next = find_next_class( blocks, monday, 825, known );
    check( line_text( next, 825, L"" ) == L"ECE 150 lec" + nl + L"PSE 5353" + dot + L"now",
           "the real timetable at 13:45 gives the now line" );
    check( line_text( next, 825, L"3.02 Linear search" ) == L"ECE 150 lec" + nl + L"3.02 Linear search",
           "the real timetable at 13:45 with a deck gives the deck line" );
    next = find_next_class( blocks, monday, 400, known );
    check( line_text( next, 400, L"" ) == L"MATH 117 tut" + nl + L"DWE 1515" + dot + L"08:30",
           "the real timetable at 06:40 gives the 08:30 line" );
    next = find_next_class( blocks, monday, 1320, known );
    check( line_text( next, 1320, L"" ) == L"MATH 115 tut" + nl + L"Tue 10:30" + dot + L"DWE 3516",
           "the real timetable at 22:00 gives the Tuesday line" );
    check( line_text( next, 1320, L"3.02 Linear search" ) == L"MATH 115 tut" + nl + L"Tue 10:30" + dot + L"DWE 3516",
           "the real timetable at 22:00 with a deck still gives the Tuesday line" );
}

}  // namespace

void test_next_line() {
    check_finding();
    check_line_today();
    check_line_other_day();
    check_line_special();
    check_line_length();
    check_line_real();
}
