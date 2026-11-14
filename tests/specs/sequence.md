# Spec: lecture decks in section order, and places

The tests go in `tests\test_sequence.cpp`, in one function `void test_sequence();`, made of `check( condition, "plain sentence" )` calls from `tests\check.h`. They use only the functions below, with file names written in the test. They create no files.

## Interface (`src\sequence.h`)

```cpp
#include <map>
#include <string>
#include <vector>
using std::wstring; using std::vector;

// The file names in section order. Names that do not start with a section number are left out.
vector<wstring> deck_order( const vector<wstring> &file_names );

// True when deck a comes before deck b. Both must start with a section number.
bool deck_before( const wstring &a, const wstring &b );

// The deck after place in ordered (a list from deck_order).
wstring deck_after( const vector<wstring> &ordered, const wstring &place );

// The file name without ".pdf", for showing.
wstring deck_label( const wstring &file_name );

std::map<wstring, wstring> read_places( const wstring &text );
wstring write_places( const std::map<wstring, wstring> &places );
wstring places_path();      // places.txt next to crammy.exe
```

And in `src\profiles.h`, `item_kind` gains `sequence`.

## The section number

A name starts with a section number when its first character is a digit. The section number is the start of the name up to the first space, or up to `.pdf` when there is no space. It is split at the dots into parts. A part is either a whole number with an optional letter after it (`18`, `18a`) or a letter on its own (`A`). A first part with no number is not a section number. `0.Welcome.pdf` has the parts `0` and `Welcome`; a part that is a word sorts as a letter part.

Two numbers compare part by part:

- Two number parts compare as whole numbers, so `9` is below `10` and `08`. When the numbers are equal, a part with no letter comes before one with a letter, and letters compare in alphabetical order, ignoring case.
- A number part comes before a letter part.
- When one number runs out of parts first and every part so far is equal, the shorter one comes first.

## `deck_order`

Given these names, in any order:

```
0.Welcome.pdf
1.01 Hello world!.pdf
1.02 Anatomy of a program.pdf
1.10 Assignment operators.pdf
1.18 Writing functions.pdf
1.18a Function tutorial.pdf
1.18b Compiler errors.pdf
1.19 Assertions.pdf
1.27 Bit-wise and -shift operators.pdf
1.A Ternary operator.pdf
2 Dynamic memory allocation.pdf
2.01 Pointers.pdf
2.05 Memory.pdf
2.05.1 Wild pointers.pdf
2.05.2 Dangling pointers.pdf
2.06 Arrays.pdf
3.08 Searching.pdf
3.9 Rotating an array to the left using reverse.pdf
3.10 Sorting.pdf
7.04 Insertion sort.pdf
7.5 Counting sort.pdf
A polynomial class.pdf
Date class.pdf
Using replit.pdf
assignment1.pdf
```

the result is the first 21 names in exactly the order listed, and the last four are not in it.

`deck_before` agrees with that order for every pair of neighbours in it, and is false for a name compared with itself.

## `deck_after`

With `ordered` the 21 names above:

- After `1.18 Writing functions.pdf` comes `1.18a Function tutorial.pdf`.
- After `1.27 Bit-wise and -shift operators.pdf` comes `1.A Ternary operator.pdf`.
- After `2.05.2 Dangling pointers.pdf` comes `2.06 Arrays.pdf`.
- After the last name, `7.5 Counting sort.pdf`, the result is `7.5 Counting sort.pdf` again.
- A place not in the list, `3.08a Gone.pdf`, gives the deck after the highest name that sorts before it: `3.08 Searching.pdf` sorts before it, so the result is `3.9 Rotating an array to the left using reverse.pdf`.
- A place that sorts before every name, `0 Start.pdf`, gives the first name, `0.Welcome.pdf`.
- An empty `ordered` gives `""`, whatever the place.

## `deck_label`

`deck_label( L"3.02 Linear search.pdf" )` is `3.02 Linear search`. A name without `.pdf` comes back as it is. The ending is matched without regard to case: `X.PDF` gives `X`.

## Places

`read_places` reads one place per line: the setup title, ` = `, then a file name. Spaces around the title and the file name are trimmed.

```
ECE 150: Lecture = 3.02 Linear search.pdf

this line has no separator
Math 117: Lecture=MATH117-L23.pdf
```

gives a map of two entries: `ECE 150: Lecture` → `3.02 Linear search.pdf`, and `Math 117: Lecture` → `MATH117-L23.pdf`. The blank line and the line without `=` are ignored. Only the first `=` separates, so a file name may hold `=`.

`write_places` writes one line per entry, `title = file`, each ending in `\r\n`, in the map's order. `read_places( write_places( m ) )` equals `m` for the map above.

`places_path()` ends with `\places.txt`.

## `sequence` lines in profiles.txt

- `read_book` on

  ```
  [X]
  root = Downloads\X
  [X: Lecture]
  full = sequence "1 Lectures\PDFs"
  ```

  gives no errors, and the setup's one item has kind `item_kind::sequence`, side `full` and the single path `1 Lectures\PDFs`. `write_book` of that book contains the line `full = sequence "1 Lectures\PDFs"`, and reading that text again gives the same item.
- A `sequence` line with two paths, `full = sequence "a" "b"`, gives an error that starts with `line ` and the line's number.
- A `sequence` line with no path gives an error in the same form.

## Also settled

- `read_places` accepts lines ending in `\n` as well as `\r\n`.
- Empty input gives empty results: `deck_order` of no names, `read_places` of empty text, and `write_places` of an empty map (`""`).
- A number part comes before a letter-only part whatever the letter's case, so `1.18` comes before `1.a`.
- A place that sorts after every name gives the last name.
- Line numbers in profile errors start at 1.
