# Spec: web pages, filling an empty side, and splits

The tests go in `tests\test_setup_options.cpp`, in one function `void test_setup_options( HWND canvas );` made of `check( condition, "plain sentence" )` calls from `tests\check.h`. `tests\test_main.cpp` calls it with the hidden canvas it already makes, after `test_existing( canvas );`. `tests\test.cmd` builds `src\side_choices.cpp`.

The interfaces are in `src\side_choices.h`, `src\canvas.h`, `src\profiles.h`, `src\launch.h` and `src\tiling.h`. Use only what those headers declare. Temporary files go under `%TEMP%\crammy option checks`, made at the start and deleted at the end. Nothing may appear on the owner's screen, nothing may start Chrome or any other program, and no test may read or write the real `profiles.txt`.

## What it is for

Three things the owner asked for:

- A setup can open web pages in Google Chrome, on a side like any other window.
- Clicking an empty part of the screen picture asks what to put there. The file and folder dialogs start in the course's folder in Downloads. The choices include "the newest PDF in a folder", for lectures, where he only picks the folder.
- The main window offers the common splits, 50 / 50, 60 / 40 and 75 / 25 and the reverse ones, without editing text.

## 1. The `web` line in profiles.txt

```
[ENGL 192]
root = %TEMP%\crammy option checks\ENGL 192

[ENGL 192: Reading]
left = pdf "a.pdf"
right = web "https://learn.uwaterloo.ca/d2l/home" "https://www.example.com/a b"
```

- `read_book` gives no errors. The setup's second item is side "right", kind `item_kind::web`, with the two addresses as written, in order.
- `all_setups` leaves the addresses exactly as written: they are not joined to the root and variables in them are not expanded.
- Writing the book and reading it again gives the same item, and the written text has the line `right = web "https://learn.uwaterloo.ca/d2l/home" "https://www.example.com/a b"`.
- `left = web` with no address gives one error starting `line N: `.
- `left = web "learn.uwaterloo.ca"` gives one error starting `line N: ` and containing `http`: an address in the file must start with `http://` or `https://` (either case).
- An unknown kind still gives its error, and that error now lists `web` among the kinds.

## 2. Opening web pages (`plan_setup`, `chrome_command`)

- `program_file( program::chrome )` is `C:\Program Files\Google\Chrome\Application\chrome.exe`.
- `chrome_command( { L"https://a.ca/x", L"https://b.ca/y z" } )` is exactly `"C:\Program Files\Google\Chrome\Application\chrome.exe" --new-window "https://a.ca/x" "https://b.ca/y z"`.
- For the setup above, with `a.pdf` made in the root, `plan_setup` gives two windows and no problems: PdfCraft on the left with `<root>\a.pdf`, and one Chrome window on the right whose paths are the two addresses, in order. Both have the setup's split.
- A setup made only of one `web` line gives one Chrome window and no problems. A web line is never checked for being a file.

## 3. Choices for an empty side (`item_for_answer`)

Let root be `%TEMP%\crammy option checks\ENGL 192` (expanded). In each case `item_for_answer( answer, side, root, item, problem )` is called with an accepted answer and side "left" unless said otherwise; it returns true, `item.side` is the side passed in, and `item.app` is pdfcraft.

- `pdf` with one path `<root>\3 Assignments\A2.pdf` gives kind pdf with the one path `3 Assignments\A2.pdf`.
- `pdf` with two paths gives kind tabs with both, relative, in order.
- `pdf` with a path outside the root, `D:\Other\x.pdf`, keeps it whole.
- `newest` with the folder `<root>\1 Lectures` gives kind newest with the one path `1 Lectures\*.pdf`.
- `newest` with the root itself gives `*.pdf`.
- `sequence`, `pick` and `folder` with the folder `<root>\1 Lectures` give those kinds with the one path `1 Lectures`. With the root itself the path is `.`.
- `web` with `learn.uwaterloo.ca/d2l` gives kind web with the one path `https://learn.uwaterloo.ca/d2l`.
- `notes` with no paths gives kind notes with no paths.
- It returns false, with a non-empty problem, for: `pdf` with no paths, `newest` with no paths, `web` with `two words`, and `web` with an empty text.

## 4. Small helpers

`web_address`:

| Typed | Gives |
|---|---|
| `https://a.ca` | `https://a.ca` |
| `HTTP://a.ca/x` | `HTTP://a.ca/x` |
| `  a.ca/x  ` | `https://a.ca/x` |
| `www.youtube.com/watch?v=1` | `https://www.youtube.com/watch?v=1` |
| `` (empty) or `   ` | `` |
| `a b.ca` | `` |

`web_label`: `https://www.example.com/a/b` gives `example.com`; `https://learn.uwaterloo.ca/d2l/home` gives `learn.uwaterloo.ca`; `http://a.ca` gives `a.ca`; `https://a.ca:8080/x?y` gives `a.ca`.

`address_in_shortcut`: the text `[InternetShortcut]\r\nURL=https://a.ca/x\r\nIDList=\r\n` gives `https://a.ca/x`; a text with no `URL=` line gives ``; `url = https://b.ca` (other case, spaces around `=`) gives `https://b.ca`.

`start_folder_for`: an existing folder gives itself; a folder that does not exist gives `%USERPROFILE%\Downloads` expanded; `""` gives the same Downloads folder.

`empty_side_at( items, across, split )`:

- With no items: `across` 0.1 gives `left`, 0.5 gives `full`, 0.9 gives `right` (thirds, as for drops).
- With only a left item and split 50: 0.7 gives `right`, 0.3 gives ``.
- With only a left item and split 75: 0.7 gives `` and 0.8 gives `right`.
- With only a right item and split 25: 0.2 gives `left`, 0.3 gives ``.
- With a full item, or with both a left and a right item, every place gives ``.

## 5. Splits

`split_choices( 50 )` gives five choices with splits 50, 60, 75, 40, 25 in that order, labelled `50 / 50`, `60 / 40`, `75 / 25`, `40 / 60` and `25 / 75`. `split_choices( 60 )` gives the same five. `split_choices( 70 )` gives those five and then `70` labelled `70 / 30`.

`set_split`, with this book:

```
[Math 115]
root = C:\Courses\Math 115

[Math 115: Exam]
split = 60

[Math 115: Lecture]

[ECE 198]
root = C:\Courses\ECE 198
split = 60
full = pick "."

[ECE 190]
root = C:\Courses\ECE 190
split = 75

[ECE 190: Lecture]
```

- `set_split( book, 0, 1, 75 )` gives Math 115: Lecture its own split 75; `all_setups` then shows 75 for it and still 60 for Exam.
- `set_split( book, 0, 0, 50 )` leaves Exam with no split of its own (`split` 0 in its `setup_block`), and `all_setups` shows 50.
- `set_split( book, 2, 0, 75 )` (ECE 190: Lecture, whose subject has 75) leaves the setup with no split of its own; `set_split( book, 2, 0, 50 )` gives it its own 50, and `all_setups` shows 50.
- `set_split( book, 1, -1, 25 )` sets the subject ECE 198's split to 25; `set_split( book, 1, -1, 50 )` leaves it at 0.
- After each of these, writing the book and reading it back gives no errors and the same splits.

## 6. The screen picture

The hidden canvas from `test_main.cpp` is used, with a `vector<written_item>` the test owns, shown with `show_items( &items, root )`, where root is the expanded `%TEMP%\crammy option checks\ENGL 192` (made as a folder). Clicks are sent as `WM_LBUTTONDOWN` then `WM_LBUTTONUP` with `SendMessageW`, at a fraction across and down the canvas's client area, as the drop checks do. A test asker is put in place with `set_side_asker` before the clicks; it records the side and start folder it was called with and how many times it was called, and returns an answer the test sets. `set_side_asker( nullptr )`, `set_canvas_split( 50 )` and `show_items( nullptr, L"" )` are called at the end.

- With no items, a click at (0.5, 0.5) calls the asker once with side `full` and start folder root. When it returns an accepted `newest` answer with the folder `<root>\1 Lectures`, the items become one: side `full`, kind newest, path `1 Lectures\*.pdf`.
- With one left item and split 50, a click at (0.8, 0.5) calls the asker with side `right`, and an accepted `web` answer with `a.ca` adds a right item of kind web with `https://a.ca`. The left item is unchanged.
- A click at (0.3, 0.5) on that same setup, on the left tile, does not call the asker and adds nothing.
- An answer with `accepted` false adds nothing.
- An answer that `item_for_answer` refuses, such as `web` with `two words`, adds nothing.
- With one left item and `set_canvas_split( 75 )`: a click at (0.7, 0.5) does not call the asker, and a click at (0.85, 0.5) calls it with side `right`.
- With one left item and `set_canvas_split( 75 )`, files dropped at (0.7, 0.5) go to the left side and files dropped at (0.85, 0.5) go to the right side. Use the drop helper's way of dropping, with a real PDF made under root.
- Dropping an Internet shortcut file (`.url`, made under root with the text `[InternetShortcut]\r\nURL=https://a.ca/x\r\n`) on an empty setup at (0.9, 0.5) adds one right item of kind web with `https://a.ca/x`.
- Every change above sends the canvas's parent `canvas_changed_message`, as drops already do; the hidden parent may ignore it.

## 7. Saving and closing

Chrome windows are not study windows: `is_study_window( L"chrome.exe", L"Chrome_WidgetWin_1" )` is false, and `app_of( L"chrome.exe" )` is `""`. Save and close all leaves the browser alone, so it never closes pages that have nothing to do with studying. (This is from `src\session.h`.)
