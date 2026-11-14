# Spec: the decks opened today

Crammy can open a `sequence` setup from the Setups window, a class click in the panel, `--setup` and Next, and these can be separate copies of `crammy.exe`. So the decks opened for each setup on a day are kept in a file, `decks today.txt` next to `crammy.exe`, and every way of opening reads it. A second open on the same day shows the same decks again; only next deck moves on.

The tests go in `tests\test_decks_today.cpp`, in one function `void test_decks_today();`, made of `check( condition, "plain sentence" )` calls from `tests\check.h`. They use only the functions below, with data built in the test. They create no files.

## Interface (added to `src\sequence.h`)

```cpp
struct decks_on_day {
    wstring date;               // "2026-11-16"
    vector<wstring> files;      // deck file names in the order they were opened, no repeats
};

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
```

File names are compared without regard to case.

## The file

One line per setup: the setup title, ` = `, the date, ` | `, then the file names separated by ` ; `.

```
ECE 150: Lecture = 2026-11-16 | 3.02 Linear search.pdf ; 3.03 Binary search.pdf
```

- `read_decks_today` of that line gives one record under `ECE 150: Lecture`, date `2026-11-16`, files `3.02 Linear search.pdf` and `3.03 Binary search.pdf` in that order.
- Spaces around the title, the date and each file name are trimmed. Lines ending in `\n` or `\r\n` both read.
- A blank line, a line without ` = `, or a line without ` | ` is ignored, and the other lines still read.
- Only the first `=` separates the title, so the rest may hold `=`.
- `write_decks_today` writes one line per record in the map's order, each ending in `\r\n`, in the form above. `read_decks_today( write_decks_today( m ) )` equals `m` for a map of two records.
- An empty map writes `""`, and empty text reads as an empty map.
- `decks_today_path()` ends with `\decks today.txt`.

## `decks_to_open`

`ordered` is the list from `deck_order`: `3.01 Arrays.pdf`, `3.02 Linear search.pdf`, `3.03 Binary search.pdf`, `3.04 Sorting.pdf`. Today is `2026-11-16`.

- Record from today with files `3.02 Linear search.pdf` and `3.03 Binary search.pdf`, place `3.03 Binary search.pdf` → those two files, in that order. The place is not moved on.
- Record from `2026-11-13` with files `3.01 Arrays.pdf`, place `3.01 Arrays.pdf` → `3.02 Linear search.pdf` only.
- Empty record, place `3.02 Linear search.pdf` → `3.03 Binary search.pdf` only.
- Empty record, no place (`""`) → empty (the tick list is shown instead).
- Record from today whose files are all gone from `ordered` (`9.99 Gone.pdf`), place `3.02 Linear search.pdf` → `3.03 Binary search.pdf` only. Files from today's record that are no longer in `ordered` are left out, and when none are left the rule falls back to the place.
- Record from today with `3.02 Linear search.pdf` and `9.99 Gone.pdf` → `3.02 Linear search.pdf` only.

## `after_opening`

- Empty record, today `2026-11-16`, files `3.02 Linear search.pdf` → date `2026-11-16`, files `3.02 Linear search.pdf`.
- That record, the same day, files `3.03 Binary search.pdf` → files `3.02 Linear search.pdf`, `3.03 Binary search.pdf`.
- That record, the same day, files `3.02 linear search.PDF` and `3.04 Sorting.pdf` → files `3.02 Linear search.pdf`, `3.03 Binary search.pdf`, `3.04 Sorting.pdf` (the repeat is left out, whatever its case).
- A record from `2026-11-13` with `3.01 Arrays.pdf`, today `2026-11-16`, files `3.02 Linear search.pdf` → date `2026-11-16`, files `3.02 Linear search.pdf` only.
- Any record with an empty list of files → the record unchanged.

## How it is used, checked by the owner on screen

- Every way of opening a `sequence` setup plans `decks_to_open` with the record for that setup from `decks today.txt` and the place from `places.txt`, unless decks were ticked, which are opened as ticked.
- After the windows open, `decks today.txt` gets `after_opening` with the decks opened, and `places.txt` gets the highest deck opened, as now.
- next deck opens the deck after the highest one in today's record (or after the place when there is none), adds it to the record, and moves the place on.
- Writes go through a `.tmp` file and `MoveFileExW`, as `places.txt` does, and both `decks today.txt` and `decks today.txt.tmp` are in `.gitignore`.
