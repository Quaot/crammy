#include "next_line.h"
#include "panel_model.h"
#include "subjects.h"

namespace {

const int days_ahead = 14;
const int soon_minutes = 60;
const size_t longest_row = 24;
const wchar_t *const day_names[] = { L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat", L"Sun" };
const wstring separator = L" · ";

bool is_leap_year( int year ) {
    return year % 4 == 0 && ( year % 100 != 0 || year % 400 == 0 );
}

calendar_date next_day( calendar_date date ) {
    static const int month_days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int length = month_days[date.month - 1];
    if ( date.month == 2 && is_leap_year( date.year ) ) length = 29;
    if ( date.day < length ) return { date.year, date.month, date.day + 1 };
    if ( date.month < 12 ) return { date.year, date.month + 1, 1 };
    return { date.year + 1, 1, 1 };
}

bool is_known( const wstring &course, const vector<wstring> &known_courses ) {
    for ( const wstring &each : known_courses )
        if ( same_course( each, course ) ) return true;
    return false;
}

wstring two_digits( int number ) {
    return ( number < 10 ? L"0" : L"" ) + std::to_wstring( number );
}

wstring clock_text( int minute ) {
    return two_digits( minute / 60 ) + L":" + two_digits( minute % 60 );
}

wstring cut_row( const wstring &row ) {
    if ( row.size() <= longest_row ) return row;
    return row.substr( 0, longest_row - 1 ) + L"…";
}

}  // namespace

next_class find_next_class( const vector<class_block> &blocks, calendar_date today, int minute_now,
                            const vector<wstring> &known_courses ) {
    next_class next;
    calendar_date day = today;
    for ( int i = 0; i <= days_ahead; i++ ) {
        vector<class_meeting> meetings = classes_on( blocks, day );
        if ( i == 0 ) {
            // Use the panel's marking, so the panel and the line agree
            today_section section = build_today( meetings, minute_now, known_courses, {} );
            for ( const today_row &row : section.rows ) {
                if ( !row.marked ) continue;
                next.found = true;
                next.meeting = row.meeting;
            }
        } else if ( !meetings.empty() ) {
            next.found = true;
            next.meeting = meetings[0];
        }
        if ( next.found ) {
            next.date = day;
            next.today = i == 0;
            next.known = is_known( next.meeting.course, known_courses );
            return next;
        }
        day = next_day( day );
    }
    return next;
}

// Two rows, like the weather on the taskbar: the class, then where and when
wstring line_text( const next_class &next, int minute_now, const wstring &deck ) {
    if ( !next.found ) return L"No classes";
    const class_meeting &meeting = next.meeting;
    wstring top = meeting.course + L" " + lower( meeting.kind ).substr( 0, 3 );
    wstring bottom;
    if ( !next.today ) {
        bottom = wstring( day_names[weekday_of( next.date )] ) + L" " + clock_text( meeting.start_minute )
                 + separator + meeting.room;
    } else if ( !deck.empty() ) {
        bottom = deck;
    } else {
        bottom = meeting.room + separator;
        int minutes = meeting.start_minute - minute_now;
        if ( minutes <= 0 ) bottom += L"now";
        else if ( minutes <= soon_minutes ) bottom += L"in " + std::to_wstring( minutes ) + L"m";
        else bottom += clock_text( meeting.start_minute );
    }
    return cut_row( top ) + L"\n" + cut_row( bottom );
}
