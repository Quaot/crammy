# Spec: saving, closing and reopening a study session

The tests go in `tests\test_sessions.cpp`, in `void test_sessions();`, made of `check()` calls from `tests\check.h`. `tests\test_main.cpp` calls it after `test_desktop_setups();`, and `tests\test.cmd` builds `src\session.cpp`. Nothing may appear on the owner's screen: any Notepad++ a test starts is started minimized without activation (`STARTF_USESHOWWINDOW`, `SW_SHOWMINNOACTIVE`) with `-multiInst -nosession -noPlugin`, and any window a test makes is placed at x = -32000, y = -32000. A test never closes, saves or touches a window it did not make, and never reads or writes the real `last session.txt`: it passes its own path.

## What it is for

Before shutting down, one action saves every notes file, remembers every study window (which files, which desktop, where on the screen), and closes them all. At the next sign-in Crammy asks whether to reopen them, and puts each one back where it was.

Study windows are every PdfCraft, Notepad++ and Acrobat window on any desktop, whether Crammy opened it or not.

## 1. The session file (`src\session.h`)

```cpp
struct saved_window {
    wstring app;            // "pdfcraft", "notepad++" or "acrobat"
    wstring desktop;        // the desktop's name, or "" when it is not known
    RECT frame{};           // the visible frame on the screen
    vector<wstring> paths;  // the files it shows, in order
};

struct saved_session {
    bool asked = false;     // the sign-in question about it has been asked
    vector<saved_window> windows;
};

wstring write_session( const saved_session &session );
saved_session read_session( const wstring &text );
```

`write_session` gives, with `\r\n` line ends:

```
asked = no

[window]
app = pdfcraft
desktop = 150
frame = 0 0 1152 1020
file = C:\Users\x\a.pdf
file = C:\Users\x\b c.pdf

[window]
app = notepad++
desktop =
frame = -9 0 969 1029
file = C:\notes.txt
```

- The first line is `asked = yes` or `asked = no`, then a blank line before each window.
- `frame` is left, top, right, bottom, separated by single spaces; numbers may be negative.
- `desktop =` with nothing after it means "".
- A window with no files still gets its block, with no `file` lines.
- An empty session is just the `asked` line.

`read_session` reads that back: `read_session( write_session( s ) )` gives the same session for any session. It also:

- ignores blank lines, lines starting with `#`, and spaces around names and values;
- ignores a line it does not understand, an `app` other than the three names, and a `frame` that does not have exactly four whole numbers (the frame stays `{0,0,0,0}`), without failing;
- treats `asked` as false unless the value is `yes`;
- drops a `[window]` block that has no `app`;
- gives an empty session for empty text.

## 2. Which windows count (`src\session.h`)

```cpp
// True for a top-level study window: Notepad++'s main window (class "Notepad++"), Acrobat's
// document window (class "AcrobatSDIWindow"), or any window of pdfcraft.exe. exe_name is
// the file name of the window's program; compared without regard to case.
bool is_study_window( const wstring &exe_name, const wstring &class_name );

// The app name used in the session file for that program: "pdfcraft", "notepad++",
// "acrobat", or "" for any other program.
wstring app_of( const wstring &exe_name );
```

| exe_name | class_name | is_study_window | app_of |
|---|---|---|---|
| notepad++.exe | Notepad++ | true | notepad++ |
| NOTEPAD++.EXE | Notepad++ | true | notepad++ |
| notepad++.exe | #32770 | false | notepad++ |
| Acrobat.exe | AcrobatSDIWindow | true | acrobat |
| Acrobat.exe | AVL_AVPopup | false | acrobat |
| pdfcraft.exe | Window Class | true | pdfcraft |
| PdfCraft.exe | anything | true | pdfcraft |
| explorer.exe | CabinetWClass | false | "" |
| notepad.exe | Notepad | false | "" |

```cpp
// The PDFs named on a program's command line, in order: every argument, as Windows splits it
// (CommandLineToArgvW), that ends in ".pdf" without regard to case. The program itself and
// options are never taken, even an option's value, unless that value ends in ".pdf".
vector<wstring> pdfs_in_command_line( const wstring &command_line );
```

- `"C:\Program Files\PdfCraft\pdfcraft.exe" --new-window --mode read "C:\a b\x.pdf" "C:\y.PDF"` gives `C:\a b\x.pdf`, `C:\y.PDF`.
- `"C:\...\Acrobat.exe" /n /A "view=FitH" "C:\s.pdf"` gives `C:\s.pdf`.
- `"C:\...\pdfcraft.exe" --control "C:\t\ctl.txt" "C:\z.pdf"` gives `C:\z.pdf`.
- A command line with no PDF gives an empty list; so does empty text.

## 3. Saving and closing (`src\session.h`)

```cpp
// Saves every file each window has open, records each window, closes it, and returns what
// was recorded. A window that cannot be closed safely is recorded but left open, and
// problems says why. Waits up to 5 seconds for the windows to go.
vector<saved_window> save_and_close( const vector<HWND> &windows, vector<wstring> &problems );

// save_and_close on every study window on every desktop, then writes the session (asked = no)
// to path. Returns the number of windows recorded.
int save_and_close_all( const wstring &path, vector<wstring> &problems );

wstring session_path();     // "last session.txt" next to profiles.txt
```

For each window, `save_and_close` records `app` (from its program), `desktop` (through `windows_by_desktop()` and `desktop_names()`), `frame` (its visible frame) and `paths`:

- **Notepad++**: every open file, from every tab of both views, in tab order. Each tab that has a file on disk is saved first. A tab with no file (a "new 1" tab) is not recorded: when it is empty it is simply closed with the window; when it has text, the window is left open with all its tabs, and problems gets `Notepad++ has an unsaved new tab with text in it. Save it yourself, then try again.`. A tab that does not save cleanly also leaves its window open, with a problem naming the file.
- **PdfCraft** and **Acrobat**: the PDFs on the command line of the window's process (`pdfs_in_command_line`).

Closing sends `WM_CLOSE`. Any window still open after 5 seconds (for example one asking about unsaved changes) gets a problem naming the app and the window title; it stays recorded.

Tests use two scratch Notepad++ processes started minimized:
- one with two scratch files written to `%TEMP%\crammy sessions check` before it starts. After `save_and_close` its process has ended within 5 seconds, it is recorded with `app` "notepad++", both files are recorded in tab order, and the files still hold what the test wrote.
- one with no files (`-multiInst -nosession -noPlugin` alone opens an empty "new 1"). After `save_and_close` it has ended and is recorded with no paths.

A minimized window's frame is off screen, so the tests do not check frames of real windows.

They also check that `save_and_close` of an empty list returns an empty list and no problems, and that a handle that is not a window is skipped with no problem.

## 4. Reopening (`src\session.h`, `src\tiling.h`)

`planned_window` gets:

```cpp
bool has_frame = false;     // when true, the window goes exactly to frame
RECT frame{};
```

In `window_targets`, a window with `has_frame` gets its `frame` as its target, and does not count towards anyone's rows. Such windows are never snapped.

```cpp
// The windows to open for a saved session, in order: one planned window per saved window
// with at least one file, with its app (pdfcraft, notepad_pp, acrobat), its files, its desktop,
// has_frame = true and its frame. side is "full" (it is not used when has_frame is set).
// A saved Acrobat window with several files gives one window per file. Files that no longer
// exist are left out; a window left with no files is left out.
vector<planned_window> reopen_plan( const saved_session &session );

// Opens reopen_plan( read_session( file at path ) ) on its desktops without switching, and
// marks the session asked. Returns problems to show.
vector<wstring> reopen_session( const wstring &path );
```

Tests for `reopen_plan` use scratch files in `%TEMP%` and check each rule above, including a missing file, a window with only missing files, Acrobat with two files, and that the desktop and frame are copied. Tests for `window_targets` check that a `has_frame` window gets its frame exactly and that two left windows on one desktop still split rows when a third left window on that desktop has a frame.

## 5. Where it is used (not covered by automatic tests)

- `crammy --save-and-close` runs `save_and_close_all( session_path(), … )` and prints the problems. `crammy --reopen` runs `reopen_session( session_path() )`. `parse_command_line` gives the actions `save-and-close` and `reopen` for these, matched without regard to case (this part is tested, in `test_sessions`).
- The tray menu gets **Save and close all** and **Reopen last session** above the separator. Reopen is greyed when there is no session file or it records no windows.
- When the background Crammy starts and the session file exists, records at least one window and has `asked = no`, it asks in a message box: *Reopen what you had open last time? N windows on desktops A, B.* Yes reopens it; either answer marks it asked, so the question comes once. The tray menu can still reopen it afterwards.
