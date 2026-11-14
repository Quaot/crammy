# Spec: when a newer download replaces open slides

The tests go in `tests\test_slide_watch.cpp`, in one function `void test_slide_watch();`, made of `check( condition, "plain sentence" )` calls from `tests\check.h`. They use only the function below, with data built in the test. They create no files.

## Interface (`src\slide_watch.h`)

```cpp
#include <string>
#include <vector>
using std::wstring; using std::vector;

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
```

## Rules

A file replaces the open one when all of these hold:

- it matches the pattern;
- it is not the open file (paths compared without regard to case);
- its modified time is later than `open_modified`;
- its size last changed at least 2000 ms ago, so a download still being written is passed over.

When several files qualify, the one with the latest modified time wins. With none, the result is `""`.

## Cases

The watch is pattern `C:\M\1 Lectures\PDF\MATH117-L*.pdf`, open path `C:\M\1 Lectures\PDF\MATH117-L23.pdf`, open modified 100.

- `MATH117-L24.pdf` in that folder, modified 200, size changed 5000 ms ago → its path.
- `MATH117-L24.pdf` modified 200 and `MATH117-L25.pdf` modified 300, both settled → the L25 path.
- `MATH117-L24.pdf` modified 300 and `MATH117-L25.pdf` modified 200, both settled → the L24 path (modified time decides, not the name).
- `MATH115-W10-Shell.pdf` in that folder, modified 200, settled → `""`.
- `MATH117-L24.pdf` in another folder, `C:\M\Other\MATH117-L24.pdf`, modified 200, settled → `""`.
- The open file itself, written as `c:\m\1 lectures\pdf\math117-l23.pdf`, modified 200, settled → `""`.
- `MATH117-L22.pdf` modified 50, settled → `""` (older than the open one).
- `MATH117-L24.pdf` modified 200, size changed 1500 ms ago → `""`; the same at 2500 ms ago → its path; at exactly 2000 ms → its path.
- An empty list → `""`.
- ECE 190: a watch on `C:\E\1 Lectures\PDF\ECE190-W*-Slides.pdf`, open `ECE190-W10-Slides.pdf` at modified 100, and files `ECE190-W11-Slides.pdf` and `ECE190-W11-Sum.pdf`, both modified 200 and settled, gives the W11 Slides path. The same files with a watch on `ECE190-W*-Sum.pdf`, open `ECE190-W10-Sum.pdf`, give the W11 Sum path.

## `matches_pattern`

- `C:\A\MATH117-L24.pdf` matches `C:\A\MATH117-L*.pdf`, and so does `c:\a\math117-l24.PDF`.
- `C:\A\MATH117-L24.pdf` does not match `C:\B\MATH117-L*.pdf`.
- `C:\A\ECE190-W11-Sum.pdf` does not match `C:\A\ECE190-W*-Slides.pdf`.
- `C:\A\X1.pdf` matches `C:\A\X?.pdf`; `C:\A\X12.pdf` does not.
- `*` may match nothing: `C:\A\L.pdf` matches `C:\A\L*.pdf`.
