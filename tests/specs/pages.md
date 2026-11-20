# Spec: opening a PDF at a page

The tests go in `tests\test_pages.cpp`, in one function `void test_pages();` made of `check( condition, "plain sentence" )` calls from `tests\check.h`. `tests\test_main.cpp` calls it after the other areas.

The interfaces are in `src\profiles.h`, `src\command_line.h`, `src\launch.h` and `src\tiling.h`. Use only what those headers declare. Temporary files go under `%TEMP%\crammy page checks`, made at the start and deleted at the end. Nothing may appear on the owner's screen, no test may start PdfCraft, Acrobat or any other program, and no test may read or write the real `profiles.txt`.

## What it is for

The owner sometimes reads a course's lecture notes in place of the slides. On 2026-11-20 that meant Thinula de Silva's Math 117 notes at page 121, and Crammy could only open the file at page 1. He wants a setup to open a PDF at a set page, and he wants to name the page when he opens a setup from the command line.

PdfCraft takes `--page N` and opens the window at that page; this was checked on 2026-11-20 with `pdfcraft-cli ui --control <file> state`, which reported page 121. Acrobat takes `/A "page=N&view=FitH"`.

## 1. `page N` on a window line in profiles.txt

```
[Math 117]
root = %TEMP%\crammy page checks\Math 117

[Math 117: Notes]
left = pdf "notes.pdf" page 121
right = newest "1 Lectures\*.pdf"
```

- `read_book` gives no errors. The first item of the setup has `page` 121; the second has `page` 0. A page of 0 means no page was given.
- `all_setups` gives the `layout_item` the same `page`: 121 for the first item, 0 for the second.
- `page N` may end a `pdf`, `newest`, `tabs`, `pick` or `sequence` line. Check a line of each kind, such as `left = tabs "a.pdf" "b.pdf" page 4`; each reads with no error and the page given.
- The word `page` ignores case: `left = pdf "a.pdf" PAGE 5` gives page 5.
- `page N` and `with acrobat` may come in either order. `left = pdf "a.pdf" page 3 with acrobat` and `left = pdf "a.pdf" with acrobat page 3` both give `viewer::acrobat`, page 3 and the one path `a.pdf`.
- Words inside the quotes are part of the path. `left = pdf "my page 3.pdf"` gives the path `my page 3.pdf` and page 0, with no error. So does `left = pdf "a with acrobat page 2.pdf"`, with `viewer::pdfcraft`.
- The number must be a whole number of 1 or more. Each of these lines gives exactly one error, starting `line N: ` (N being its line number) and containing the word `page`, and the line adds no item: `left = pdf "a.pdf" page 0`, `left = pdf "a.pdf" page x`, `left = pdf "a.pdf" page`, `left = pdf "a.pdf" page -2`, `left = pdf "a.pdf" page 1.5`.
- A page only goes on a line that opens PDFs. `left = notes page 2`, `left = folder "x" page 2` and `left = web "https://a.com" page 2` each give exactly one error starting `line N: ` and containing `page`, and add no item.
- Writing: `write_book` writes the page after the paths and before `with acrobat`. An item read from `left = pdf "a.pdf" page 121` is written as the line `left = pdf "a.pdf" page 121`. One read from `left = pdf "a.pdf" with acrobat page 3` is written as `left = pdf "a.pdf" page 3 with acrobat`. An item with page 0 is written with no `page` word.
- Reading what was written gives the same items again, pages included.

## 2. `--page` on the command line

`parse_command_line` takes the words after the program name. `setup_request` has `int page`, 0 when no page was given.

- `--setup "Math 117: Reading" --pick MATH117-Notes-v3.pdf --page 121` gives one setup with the pick `MATH117-Notes-v3.pdf`, page 121 and no error.
- The order does not matter: `--setup A --page 4 --pick x` gives the pick `x` and page 4.
- A page belongs to the `--setup` before it: `--setup A --page 5 --setup B` gives A page 5 and B page 0.
- `--page 7` with no `--setup` before it gives the error `--page needs a --setup before it.`
- `--setup A --page`, `--setup A --page x`, `--setup A --page 0` and `--setup A --page -3` each give an error that contains `--page` and `page number`.

## 3. Planning the windows

`plan_setup( chosen, picks, separate, problems, page )` has a last parameter `int page`, 0 when left out. `planned_window` has `int page`, 0 when it is not set.

Make PDFs under the temporary folder and a setup through `read_book` and `all_setups` with its root there.

- With page 0, each PdfCraft and Acrobat window gets the page of its line: a `pdf` line with `page 121` gives a window with page 121, and a line with no page gives page 0.
- With a page of 9, every PdfCraft and Acrobat window gets page 9, whatever its line says.
- Notes (Notepad++), folder (Explorer) and web (Chrome) windows always have page 0, with or without the parameter.
- A line that gives one window for each PDF gives each of them the page: a `pdf` line `with acrobat page 3`, and a `pick` line with two ticked PDFs and `separate` true.
- Leaving the parameter out gives the same plan as passing 0.

## 4. The commands that start the programs

`pdfcraft_command( paths, control_file, page )` and `acrobat_command( path, page )` take a last parameter `int page`, 0 when left out. `quoted` and `program_file` are in `tiling.h`.

- `pdfcraft_command( { "a.pdf" }, "", 121 )` is exactly `quoted( program_file( program::pdfcraft ) ) + L" --new-window --mode read --page 121 \"a.pdf\""`.
- `pdfcraft_command( { "a.pdf" }, "C:\\t\\ctl.txt", 2 )` is exactly the start above with ` --control "C:\t\ctl.txt" --page 2 "a.pdf"` after `--mode read`.
- With page 0, or with the parameter left out, neither command has `--page` or `page=`, and both are exactly what they were before: `pdfcraft_command( { "a.pdf" }, "" )` ends `--mode read "a.pdf"`, and `acrobat_command( "a b.pdf" )` is `quoted( program_file( program::acrobat ) ) + L" /n /A \"view=FitH\" \"a b.pdf\""`.
- `acrobat_command( "a b.pdf", 121 )` is exactly `quoted( program_file( program::acrobat ) ) + L" /n /A \"page=121&view=FitH\" \"a b.pdf\""`.

`open_windows` starts each window with its `page`. That cannot be checked without starting a program, so it has no test; the owner's run of a setup checks it.
