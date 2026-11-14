#pragma once

#include "timetable.h"

struct next_class {
    bool found = false;     // false when no class is found in the days looked at
    calendar_date date;     // the day of the class
    class_meeting meeting;
    bool today = false;     // the class is on the day passed as today
    bool known = false;     // the course is one of known_courses
};

// Today's marked class, else the first class of the next day with classes (up to 14 days ahead)
next_class find_next_class( const vector<class_block> &blocks, calendar_date today, int minute_now,
                            const vector<wstring> &known_courses );

// The one line of text for the taskbar
wstring line_text( const next_class &next, int minute_now, const wstring &deck );
