# Spec: setting up several desktops without showing them

The tests go in `tests\test_desktop_setups.cpp`, in `void test_desktop_setups();`, made of `check()` calls from `tests\check.h`. `tests\test_main.cpp` calls it after `test_decks_today();`. Nothing may appear on the owner's screen: any window a test makes is placed at x = -32000, y = -32000, and any Notepad++ a test starts is started minimized without activation (`STARTF_USESHOWWINDOW`, `SW_SHOWMINNOACTIVE`). Every window and process a test makes is closed before the test returns, and a test that moves its own window to another desktop moves it back before destroying it.

## Why

Running `crammy --setup` for three subjects one after another went wrong in two ways:

- Notepad++ keeps a single window. `notepad++.exe "file"` only added each notes file as a tab in the window already open on the first desktop, so the other desktops got no notes window. Closing the old notes then switched through that window's tabs where you could see it, and closed the notes of the setups that had just opened.
- Crammy had to show each setup's desktop before opening windows there, so setting up three desktops meant watching three switches.

## 1. Moving a window to a desktop (`src\desktops.h`)

```cpp
// Moves a top-level window to the desktop with this name, without showing that desktop.
// Returns "" when the window is on that desktop afterwards, or what went wrong.
wstring move_window_to_desktop( HWND window, const wstring &name );
```

- It works for windows of any process, not only Crammy's own.
- After it returns "", `windows_by_desktop()` reports the window on the desktop with that name (its `desktop_index` is that name's index in `desktop_names()`), and `current_desktop_index()` is unchanged: the desktop showing is still the one that was showing before.
- Moving a window to the desktop it is already on returns "" and changes nothing.
- A name that matches no desktop (compared without regard to case) returns a message that contains the name in single quotes, and the window stays where it was.
- A handle that is not a window (for example `nullptr`, or a window already destroyed) returns a non-empty message and does nothing else.
- A window Explorer has not put on any desktop yet (an application window made but never shown) returns a message containing `not on any desktop yet`, and is not moved. Once it has been shown (`SW_SHOWNOACTIVATE`) and Explorer reports it on the desktop showing, moving it to another desktop returns "". Crammy relies on this: a new program's window can appear a moment before Explorer takes it in, so a move that fails is tried again on the next pass until the windows settle, and only reported if it never works.

Test with a window of the test program: an ordinary visible application window (`WS_EX_APPWINDOW`, no `WS_EX_TOOLWINDOW`, no `WS_EX_NOACTIVATE`), off screen, 200 by 100, unique title, shown with `SW_SHOWNOACTIVATE`. When there are fewer than two desktops, the tests that need a second desktop pass with a check that says they were skipped. Pick as the other desktop any desktop that is not the one showing.

## 2. Starting Notepad++ (`src\tiling.h`)

```cpp
// The command line that starts Notepad++ on one file, in a window of its own.
wstring notepad_command( const wstring &path );
```

It returns exactly `quoted( program_file( program::notepad_pp ) ) + L" -multiInst -nosession " + quoted( path )`. Crammy uses it for every notes window, so each setup's notes get a separate Notepad++ window that can sit on that setup's desktop.

## 3. Closing old notes (`src\notes_tabs.h`)

```cpp
void close_old_notes( const vector<wstring> &keep, HWND window = nullptr );
```

What already holds stays true: only files Crammy remembered (with `remember_notes`) and that are not in `keep` are closed; a file that does not save cleanly stays open; tabs of files you opened yourself are never closed.

New:

- With `window` left as `nullptr`, it works on **every** Notepad++ main window (class name `Notepad++`) that is open, not just the first one found.
- When a remembered, not-kept file is the only file open in its Notepad++ window, the file is saved and that whole window closes, so no empty window is left behind. Its process ends within 5 seconds.
- A window whose only file is in `keep` stays open with its one file.
- A window with several files keeps the old behaviour: only the matching tabs close.
- After it returns, `notes opened.txt` lists only the remembered files that are still open.

Test with two separate Notepad++ processes started minimized, each with `-multiInst -nosession -noPlugin` and one scratch file in `%TEMP%\crammy desktop setups check`: one file remembered and not kept, the other remembered and kept. Call `close_old_notes( { kept }, nullptr )`. Check that the first process has ended within 5 seconds and that the second window still exists with exactly one file open (`SendMessageW( window, WM_USER + 1000 + 7, 0, 1 ) == 1`). Then close the second one. Scratch files are deleted at the end. Pass `keep` so that it also holds any file Notepad++ already had open before the test started, so the owner's own notes are never touched: list the remembered files with `remember_notes` only for the two scratch files.

## 4. The command line (`src\command_line.h`, new)

```cpp
struct setup_request {
    wstring title;              // the text after --setup
    vector<wstring> picks;      // the texts after --pick that follow this --setup
    bool separate = false;      // --separate after this --setup
};

struct command_line {
    wstring action;             // "", "list", "check", "edit", "background", "panel"
    wstring list_title;         // --list "title"
    vector<setup_request> setups;
    bool show = false;          // --show
    wstring error;              // what is wrong with the arguments, or ""
};

// arguments are the words after the program name
command_line parse_command_line( const vector<wstring> &arguments );
```

- Options are matched without regard to case.
- `--setup "T"` starts a new request with title T. `--setup` can appear more than once; requests keep their order.
- `--pick a b c` adds every following word that does not start with `-` to the picks of the latest `--setup`. `--separate` sets `separate` on the latest `--setup`.
- `--pick` or `--separate` with no `--setup` before it sets `error` to `"--pick needs a --setup before it."` or `"--separate needs a --setup before it."`.
- `--setup` with no title after it (end of the words, or a word starting with `-`) sets `error` to `"--setup needs a setup name after it."`.
- `--list` sets `action` to `"list"`, and takes the next word as `list_title` when it does not start with `-`. `--check`, `--edit`, `--background`, `--panel` set `action` to that word without the dashes. `--show` sets `show`.
- Any other word sets `error` to `"Unknown option: "` followed by the word as given. The first error found is kept.
- No words: everything empty, no error.

Examples:

| words | result |
|---|---|
| `--setup "ECE 150: Exam" --pick F24-F` | one request, picks `F24-F` |
| `--setup "Math 115: Exam" --pick F23-F F23-M --separate` | one request, two picks, separate |
| `--setup A --pick x --setup B --setup C --pick y z` | three requests: A with `x`, B with none, C with `y`, `z` |
| `--SETUP A --PICK x` | one request A with `x` |
| `--pick x` | error `--pick needs a --setup before it.` |
| `--setup` | error `--setup needs a setup name after it.` |
| `--list "ECE 150: Exam"` | action `list`, list_title set, no requests |
| `--wobble` | error `Unknown option: --wobble` |

## 5. Opening setups on their desktops (`src\launch.h`, `src\main.cpp`)

Not covered by automatic tests, since it opens real program windows; it is checked by hand once on the owner's machine with `windows_by_desktop()`.

```cpp
struct setup_to_open {
    const setup *chosen;
    vector<vector<wstring>> picks;   // as for open_setup
    bool separate;
};

// Opens each setup on its own desktop while the desktop showing stays the same, then shows
// the desktop of the first setup that names one. Returns problems to show.
vector<wstring> open_setups( const vector<setup_to_open> &setups );
```

- All notes files of all the setups are made first, and `close_old_notes` runs once with all of them in `keep`, so opening one setup never closes the notes of another opened in the same run.
- For a setup whose desktop is not the one showing, its windows start on the desktop showing and each one is moved with `move_window_to_desktop` as soon as it appears, then sized to its side of the screen directly (no Win+arrow snapping, no keys pressed, nothing brought to the front), since keys and focus would act on the desktop showing.
- For a setup on the desktop showing, or with no desktop, windows open and snap as before.
- When the setup names a desktop that does not exist, its windows open on the desktop showing and that is reported. A window that cannot be moved stays on the desktop showing, and that is reported too.
- After every setup has opened, the desktop of the first setup that names a desktop is shown, once.
- `crammy --setup A --pick … --setup B --pick …` uses `open_setups`. With one `--setup`, it behaves as before except that it also opens without switching first and shows the desktop once at the end.

## 6. Opening every setup at once (`src\tiling.h`)

`open_setups` starts the programs for all its setups together, then moves and places each window as it appears, so the time taken is about that of the slowest program, not the sum of them all. Before, it opened one setup, waited for every window to settle, then started the next.

`planned_window` gets one more field:

```cpp
wstring desktop;        // the desktop this window goes to, or "" for the desktop showing
```

`open_windows_recorded` honours it: a window with a desktop is moved there as it appears and sized without keys or focus, as section 5 describes. Windows with no desktop are placed as before, snapped with Win+arrow after the others are in place.

Where each window goes on the screen is worked out by a function with no side effects, so it can be tested:

```cpp
// The part of the screen each planned window should fill, in plan order. Windows that share a
// side split it into rows, top to bottom in plan order, but only with windows going to the same
// desktop: windows on different desktops never share rows.
vector<RECT> window_targets( const vector<planned_window> &plan, const RECT &screen );
```

- `left` is the left half of `screen`, `right` the right half, `full` all of it. The halves meet at `screen.left + width / 2` with integer division.
- n windows on the same side and desktop each get a row of height `height / n` (integer division), in plan order; the last row runs to `screen.bottom`.
- Desktop names are compared without regard to case.
- An empty plan gives an empty result.

Examples with `screen = { 0, 0, 1000, 600 }`:

| plan (side, desktop) | targets |
|---|---|
| (left, "") , (right, "") | {0,0,500,600}, {500,0,1000,600} |
| (full, "105"), (full, "192") | {0,0,1000,600}, {0,0,1000,600} |
| (left, "150"), (left, "150"), (left, "190") | {0,0,500,300}, {0,300,500,600}, {0,0,500,600} |
| (right, "150"), (right, "150"), (right, "150") | {500,0,1000,200}, {500,200,1000,400}, {500,400,1000,600} |
| (left, "A"), (left, "a") | {0,0,500,300}, {0,300,500,600} |

With `screen = { 100, 50, 1101, 651 }`, (left, "") gives {100,50,600,651} and (right, "") gives {600,50,1101,651}.

The tests for this go in the same `tests\test_desktop_setups.cpp`, in a function called from `test_desktop_setups()`. They only call `window_targets`; they open no windows.

## 7. Uneven sides (`split`)

A subject or a setup can say how much of the screen width its left side takes:

```
[ECE 150]
root = Downloads\ECE 150
desktop = 150
split = 75

[ECE 150: Exam]
left = pick "6 Exams" "2 Practice"
right = notes
```

Here the PDF fills the left 75% and the notes the right 25%, like the 75/25 snap layout Windows offers.

### Reading and writing (`src\profiles.h`)

- `profile_block`, `setup_block` and `setup` each get `int split`. In the blocks, 0 means the line is missing. In `setup` it is the value that applies, 50 when neither the setup nor its subject sets one.
- `split = N` is read like `desktop`: under a subject heading it goes to the subject, under a setup heading to that setup. A setup's own value wins over its subject's.
- N must be a whole number from 10 to 90. Anything else (empty, `abc`, `9`, `91`, `75%`, `50.5`) adds the error `line L: split must be a whole number from 10 to 90` and leaves the value at 0. Spaces around the number are allowed.
- `write_book` writes `split = N` after the `desktop` line of the subject or setup that has it, and nothing when it is 0, so a file without split lines reads and writes back unchanged.
- The comment block at the top of a written file gains the line `#   split = 75                   the left side takes 75% of the width (default 50)`.

### Placing windows (`src\tiling.h`)

`planned_window` gets `int split = 50;`, filled from the setup by `plan_setup`. In `window_targets`, the line between left and right for a window is at `screen.left + width * split / 100` (integer arithmetic), using that window's own split. `full` ignores it. Rows split each side as before. With split 50 every result is the same as before.

Windows with a split other than 50 are never snapped with Win+arrow, since Windows only snaps to halves: they are moved straight to their target.

Examples with `screen = { 0, 0, 1000, 600 }`:

| plan (side, desktop, split) | targets |
|---|---|
| (left, "", 75), (right, "", 75) | {0,0,750,600}, {750,0,1000,600} |
| (left, "", 25), (right, "", 25) | {0,0,250,600}, {250,0,1000,600} |
| (full, "", 75) | {0,0,1000,600} |
| (right, "150", 75), (right, "150", 75) | {750,0,1000,300}, {750,300,1000,600} |
| (left, "150", 75), (left, "190", 50) | {0,0,750,600}, {0,0,500,600} |

With `screen = { 0, 0, 1001, 600 }`, (left, "", 75) gives {0,0,750,600}.

### Tests

In `tests\test_desktop_setups.cpp`: reading split on a subject, on a setup, a setup overriding its subject, the default of 50 in `setup` (through `all_setups`), every bad value listed above giving the error with the right line number, spaces around the number, `write_book` round trips with and without split lines, the new comment line, and every example in the table. They use `read_book`, `write_book` and `all_setups` on text in memory and never touch `profiles.txt`.

## 8. Read mode, fit to width (`src\tiling.h`)

Every PDF Crammy opens starts in PdfCraft's Read mode, fitted to the width of its window. PdfCraft takes the mode at launch (`--mode read`), which beats its own default for that window, so no PdfCraft settings file is touched. With no `--zoom`, PdfCraft fits the page width, its own default. (An earlier version passed `--zoom 80`; the owner asked for fit to width in its place.) Acrobat is told to fit the width through its open parameters.

```cpp
// The command line that starts PdfCraft on these PDFs in a window of its own, in Read mode,
// fitted to width. control_file, when not empty, adds --control and that file.
wstring pdfcraft_command( const vector<wstring> &paths, const wstring &control_file );

// The command line that starts Acrobat on one PDF in a new window, fitted to width.
wstring acrobat_command( const wstring &path );
```

- `pdfcraft_command( { "a.pdf", "b c.pdf" }, "" )` is exactly `quoted( program_file( program::pdfcraft ) ) + L" --new-window --mode read \"a.pdf\" \"b c.pdf\""`.
- With a control file `C:\t\ctl.txt` it is `quoted( program_file( program::pdfcraft ) ) + L" --new-window --mode read --control \"C:\t\ctl.txt\" \"a.pdf\""` for one PDF `a.pdf`.
- Neither contains `--zoom`.
- `acrobat_command( "a b.pdf" )` is exactly `quoted( program_file( program::acrobat ) ) + L" /n /A \"view=FitH\" \"a b.pdf\""`.
- Crammy starts every PdfCraft and Acrobat window with these.

Tests in `tests\test_desktop_setups.cpp` compare the strings only and start nothing.
