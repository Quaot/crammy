# Spec: the next class, and its line on the taskbar

The tests go in `tests\test_next_line.cpp`, in one function `void test_next_line();`, made of `check( condition, "plain sentence" )` calls from `tests\check.h`. They read the repo's real `timetable.txt`, one folder above the test program: the folder holding `tests\crammy_test.exe`, then `..\timetable.txt`, through `read_timetable`. Some cases build their own `class_block` lists.

## Interface (`src\next_line.h`)

```cpp
#include "timetable.h"   // calendar_date, class_block, class_meeting

struct next_class {
    bool found = false;     // false when no class is found in the days looked at
    calendar_date date;     // the day of the class
    class_meeting meeting;
    bool today = false;     // the class is on the day passed as today
    bool known = false;     // the course is one of known_courses (compared with same_course)
};

next_class find_next_class( const vector<class_block> &blocks, calendar_date today, int minute_now,
                            const vector<wstring> &known_courses );

wstring line_text( const next_class &next, int minute_now, const wstring &deck );
```

Minutes are counted from midnight: 13:30 is 810.

## `find_next_class`

The known courses are ECE 105, ECE 150, ECE 190, ECE 198, ENGL 192, `Math 115` and `Math 117` (written that way to check that case is ignored).

- On today's date, the class is the one marked by the panel's rule: a class that is on now (start ≤ minute < end) stays the class until it ends; otherwise it is the first class that has not started. That is, it agrees with `build_today` in `panel_model.h`.
- When no class today is on or still to come, the class is the first class, in start order, of the next day that has any. Days are looked at one at a time, for at most 14 days after today. With nothing in those days, `found` is false.

With the real timetable, on Monday 2026-11-16:

| minute | class | today |
|---|---|---|
| 400 (06:40) | MATH 117 TUT, 510-620, DWE 1515 | true |
| 790 (13:10) | MATH 117 LEC, 750-800, PSE 5353 | true |
| 805 (13:25) | ECE 150 LEC, 810-860, PSE 5353 | true |
| 825 (13:45) | ECE 150 LEC | true |
| 1320 (22:00) | MATH 115 TUT on Tuesday 2026-11-17, 630-740 (10:30-12:20), DWE 3516 | false |

- At 805, `known` is true; for a GENE 119 class found the same way, `known` is false.
- Saturday 2026-11-21 at 600 gives MATH 117 TUT on Monday 2026-11-23, 510, with `today` false.
- A list with one block, `ECE 150 | LEC | M | 13:30-14:20 | PSE 5353 | 2026-12-28 to 2026-12-28`, asked on Monday 2026-12-07 at 600, gives `found` false: Dec 28 is 21 days on.
- An empty list gives `found` false.

## `line_text`

The line has two rows, like the weather it replaces: a top row naming the class and a bottom row saying where and when. `line_text` returns them joined by one newline, `
` (U+000A), with no newline at the end. Parts within a row are joined with ` · ` (space, middle dot U+00B7, space). The kind is written out as a word with a capital first letter: `LEC` gives `Lecture`, `TUT` gives `Tutorial`, `LAB` gives `Lab`, `TST` gives `Test` and `SEM` gives `Seminar`. Any other code is lower-cased with a capital first letter, so `PRJ` gives `Prj`.

The top row is always `<course> <kind>`.

The bottom row, for a class today:

- When it is on: `<room> · now`.
- When it starts within 60 minutes: `<room> · in <N>m`, where N is the start minute minus the minute now.
- Otherwise: `<room> · <HH:MM>`, its start time with two digits for the hour.
- When `deck` is not empty, the deck is the whole bottom row: `<deck>`.

The bottom row, for a class on another day: `<Ddd> <HH:MM> · <room>`, where Ddd is `Mon`, `Tue`, `Wed`, `Thu`, `Fri`, `Sat` or `Sun`. The deck is not shown for another day.

When `found` is false the line is the single row `No classes`, with no newline.

Each row longer than 24 characters is cut to its first 23 characters followed by `…` (U+2026), for 24 in all. Rows of 24 or fewer are left as they are.

With the real timetable on Monday 2026-11-16:

- 805 gives `ECE 150 Lecture
PSE 5353 · in 5m`.
- 825 gives `ECE 150 Lecture
PSE 5353 · now`.
- 400 gives `MATH 117 Tutorial
DWE 1515 · 08:30`.
- 1320 gives `MATH 115 Tutorial
Tue 10:30 · DWE 3516`.
- 825 with the deck `3.02 Linear search` gives `ECE 150 Lecture
3.02 Linear search`.
- 1320 with the deck `3.02 Linear search` still gives `MATH 115 Tutorial
Tue 10:30 · DWE 3516`, because that class is not today.

A next_class made in the test, today, on now, course `ECE 150`, kind `LEC`, with the deck `1.08 Conditional statements and comparison operators`, gives `ECE 150 Lecture
` followed by a bottom row of exactly 24 characters: the first 23 characters of the deck, then `…`.

## Also settled

- A class exactly 60 minutes away counts as within 60 minutes: `in 60m`. At 61 minutes the start time shows.
- Characters are counted as `wstring` characters, so the middle dot and the ellipsis count as one each, and each row is counted on its own.
- When `today` is true, `date` is the day passed as today.
- With `found` false the line is `No classes`, whatever the deck.
- On Monday 2026-11-16 at 625 (10:25), MATH 117 TUT has ended and the class is GENE 119 SEM at 630, with `known` false.
