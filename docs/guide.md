# Working on Crammy

This guide is for anyone changing Crammy. Read it once before touching the code. The README covers using it; this covers how it works and how to change it without breaking it.

## The short version

Crammy is a Win32 program in C++17 with no libraries beyond Windows itself. It reads `profiles.txt`, shows a window where setups can be built by dragging files, and opens a setup by starting PdfCraft, Adobe Acrobat, Notepad++ or File Explorer and moving each window to its place on the screen.

It can also run in the background with a tray icon and a keyboard shortcut. From there it shows a panel with today's classes from `timetable.txt` and a chip for each virtual desktop. Links at the bottom of the panel start Youtonomous or bring up the setups.

Paths in this guide are relative to the repo folder unless they start with a drive letter. The owner's machine is set up like this:

| What | Where |
|---|---|
| Source | `C:\Users\Justin\Desktop\Projects\Crammy` (repo `github.com/Quaot/crammy`, branch `main`) |
| The program | `crammy.exe` in the repo folder, next to `profiles.txt`; built, not committed |
| Desktop shortcut | `C:\Users\Justin\Desktop\Crammy.lnk`, pointing at `crammy.exe` |
| `settings.txt` | Next to `crammy.exe`; not committed. It holds the panel shortcut and the Youtonomous program and videos. It also says whether to start at sign-in. Every key has a default, so the file may be missing |
| `timetable.txt` | Next to `crammy.exe`; committed. One line for each class block, copied from Quest |
| `places.txt` | Next to `crammy.exe`; not committed. The last lecture deck reached for each `sequence` setup, such as `ECE 150: Lecture = 3.02 Linear search.pdf` |
| `decks today.txt` | Next to `crammy.exe`; not committed. The lecture decks opened for each `sequence` setup on its last day of use, in the order they were opened, such as `ECE 150: Lecture = 2026-11-16 \| 3.02 Linear search.pdf ; 3.03 Binary search.pdf` |
| Startup shortcut | `Crammy.lnk` in the user's Startup folder, running `crammy.exe --background`; made and removed by the tray menu |
| Windhawk mod | `windhawk\crammy-button.wh.cpp`, added to Windhawk by hand; it shows the current desktop's name and Crammy's line in two rows, with the faint `›` next button, on the taskbar |
| Course files | `C:\Users\Justin\Downloads\<course>`, such as `Downloads\ECE 190`. File names follow `Downloads\Index.md` (`ECE190-W10-Slides.pdf`, `MATH117-L23.pdf`) |
| Compiler | MSYS2 UCRT64 g++ at `C:\msys64\ucrt64\bin` |

## Build and check

Run these from the repo folder, with `.\` in front. This machine sets `NoDefaultCurrentDirectoryInExePath`, so a bare `build.cmd` is not found.

```
.\build.cmd                 builds crammy.exe
.\tests\test.cmd            builds tests\crammy_test.exe and runs every check
.\crammy.exe --check        reads profiles.txt and reports problems
```

Run all three after every change. `build.cmd` uses `-Wall`, and the code builds with no warnings; keep it that way. A new source file goes into the file lists of both `build.cmd` and `tests\test.cmd`.

The Testing section below says how the checks are written and what they need on this machine.

To try a setup for real, run it from a terminal: `crammy.exe --setup "Math 117: Lecture"`. Only do this when the owner has asked for something to be opened, since it switches desktops and opens windows.

## How a setup opens

This is the path through the code, in order:

1. `main.cpp` reads the command line. With `--setup` and either `--pick` or a setup that has nothing to pick, it opens the setup straight away; otherwise it shows the main window.
2. `profiles.cpp` reads `profiles.txt` into a `profile_book`, which keeps each subject with its setups and every window line as written. `all_setups` turns that into a list of `setup` values with every path joined to the subject's root.
3. If the setup has `pick` lines, or a `sequence` line with no saved place, `dialogs.cpp` shows a tick list and returns the PDFs chosen.
4. `launch.cpp` (`open_setup`) turns each line into a `planned_window`, which says what program to start on which side of the screen with what files. `newest` lines find their file here, and a warning is added when that file is not from today. A `notes` line gets its file name from `files.cpp` (`notes_file`), named after the first PDF of the setup.
5. `notes_tabs.cpp` (`close_old_notes`) saves and closes, in Notepad++, the notes files Crammy opened for earlier setups.
6. `launch.cpp` (`open_setups`) marks each planned window with its setup's desktop, or "" when that desktop is the one showing, and hands every window of every setup to `tiling.cpp` in one list. One notes clean-up runs first for all of them.
7. `tiling.cpp` (`open_windows_recorded`) starts every program at once, finds each new window, moves a window that has a desktop there with `move_window_to_desktop` as soon as Explorer has put it on a desktop, and sizes it to its place. Windows for the desktop showing are snapped afterwards. Then `desktops.cpp` (`switch_to_desktop`) shows the first setup's desktop once. Filling four desktops takes about four seconds.
8. `launch.cpp` (`save_places`) adds the decks opened for a `sequence` line to the setup's record in `decks today.txt`, and saves the highest of them as the setup's place in `places.txt`.

Problems at any step are collected as plain sentences and shown at the end; nothing stops halfway with an error.

## Source files

| File | Job |
|---|---|
| `src/main.cpp` | Command line, then the main window or a direct open |
| `src/main_window.cpp` | The main window: setup list, New setup / Rename / Delete, desktop choice, Add notes, Edit as text, Open |
| `src/canvas.cpp` | The screen picture: drawing tiles, drag and drop from File Explorer, dragging tiles, the right-click menu |
| `src/dialogs.cpp` | The New setup / Rename dialog and the tick list shown before opening; the tick list is the only place answer keys are hidden |
| `src/editor_window.cpp` | Edit as text: a plain editor for `profiles.txt` |
| `src/profiles.cpp` | Reading and writing `profiles.txt`; joining paths to the root |
| `src/sequence.cpp` | Putting numbered lecture decks in section order and choosing the decks to open (`decks_to_open`), with the reading and writing of `places.txt` and `decks today.txt` |
| `src/next_line.cpp` | Which class the taskbar line is about (`find_next_class`) and the text of the line (`line_text`) |
| `src/slide_watch.cpp` | Deciding whether a newer `newest` file should replace the one open (`replacement_for`) |
| `src/next_runner.cpp` | What Next does in the background: the windows it opened this run, the watches on newest slides, the decks opened today, `next deck`, and the line kept as the background window's title |
| `src/files.cpp` | Finding PDFs, spotting answer keys, the `newest` pattern, naming notes files |
| `src/launch.cpp` | Turning a setup into windows to open |
| `src/tiling.cpp` | Starting programs, finding their windows, moving and snapping them; small helpers shared with other files |
| `src/notes_tabs.cpp` | Closing old notes in every Notepad++ window |
| `src/command_line.cpp` | Reading the command line, including several `--setup` groups |
| `src/session.cpp` | Save and close all, the session file, and reopening it |
| `src/desktops.cpp` | Reading virtual desktop names, switching between them, finding which desktop each window is on, and creating a desktop |
| `src/timetable.cpp` | Reading `timetable.txt` and working out which classes meet on a date, with the date and weekday helpers |
| `src/settings.cpp` | Reading and writing `settings.txt` and the shortcut text, and building the command that starts Youtonomous |
| `src/subjects.cpp` | Matching a class to a desktop name and to a setup, plus two small helpers: `kind_name`, the word for a class kind (only used to fill `today_row`), and the check for a course folder in Downloads |
| `src/panel_model.cpp` | What the panel shows and does, as plain data with no window: today's rows and the desktop tiles, and the plan for a class click (the types are in `panel_model.h`) |
| `src/panel.cpp` | The panel window; `toggle_panel` in `panel.h` shows it or hides it, and `show_panel` only shows it |
| `src/background.cpp` | Background mode: the hidden `CrammyBackground` window and its messages, the tray icon, the shortcut, the one-copy lock and the Startup shortcut |
| `src/ui.cpp` | Fonts, DPI scaling, window creation, the message loop |
| `src/text.cpp` | String, path and file helpers |
| `tests/test_main.cpp` | The older checks, and the calls to every `test_<area>()`; writes a picture when asked |
| `tests/check.cpp`, `tests/check.h` | `check()`, which prints one line and counts a failure |
| `tests/test_<area>.cpp` | The checks for one area, written from `tests/specs/<area>.md`: `existing`, `timetable`, `settings`, `subjects`, `panel`, `background`, `desktops`, `sequence`, `next_line`, `slide_watch`, `desktop_setups` and `sessions` |
| `tests/specs/*.md` | One spec for each area |
| `tests/test.cmd` | Builds the checks and runs them |
| `windhawk/crammy-button.wh.cpp` | The Windhawk mod that shows Crammy's line on the taskbar, with the current desktop's name before it, plain, with a thin line after it; `windhawk/README.md` says how to add it |
| `art/make_icon.py` | Draws `crammy.ico` with Pillow (`python art\make_icon.py`) |

Answer keys are hidden from the tick list until its "Show answer keys" box is ticked. `is_answer_key` in `files.cpp` decides what counts as a key, with one case-insensitive pattern: the name contains "answer key", the word "solution" or "solutions", `-M-S (` or `-F-S`, or its stem ends in `-S` (the renamed course files, such as `ECE150-Mock-1112a-S.pdf`). `--pick` and `--list` do not use it.

## The profile file

`profiles.txt` sits next to `crammy.exe`. The main window rewrites the whole file on every change, using `write_book` in `profiles.cpp`. Only the header comment at the top survives a rewrite; comments written anywhere else are lost. The tests check that reading and writing the file loses nothing else.

```
[ECE 190]
root = Downloads\ECE 190
desktop = 190

[ECE 190: Lecture]
left = newest "1 Lectures\PDF\ECE190-W*-Slides.pdf"
right = newest "1 Lectures\PDF\ECE190-W*-Sum.pdf" with acrobat
```

- `[Subject]` starts a subject; `[Subject: Setup]` adds a setup to it. A subject with no setups opens as it is and is listed under its own name.
- Subject settings: `root` (relative to the user folder), `desktop` (a virtual desktop name), `notes folder` (relative to the root; the default is `2 Practice` when it exists, else the root), `split` (the left side's share of the width in percent, a whole number from 10 to 90, 50 when missing). A setup may set its own `notes folder`, `desktop` or `split`. The exam and worksheet setups with notes use `split = 60`, with the paper on the left and the notes on the right.
- Window lines are `side = kind "path" "path" ...`. `side` is `left`, `right` or `full`. Lines under a subject heading are shared by all its setups.
- Kinds: `notes` (Notepad++; no path means a file named after the first PDF), `pdf` (one file), `newest` (one pattern, newest file by modified date), `tabs` (several PDFs in one PdfCraft window), `pick` (folders or files to tick from), `sequence` (the numbered decks of one folder in section order; opens the decks already opened today, as listed in `decks today.txt`; on a new day it opens the one after the place saved in `places.txt`, or shows the tick list when there is no place), `folder` (File Explorer).
- A PDF line may end with `with acrobat`. Acrobat gets one window per PDF.
- Paths are stored relative to the root when they are inside it. `*` works in file names. `%USERPROFILE%` and other variables are expanded.

To add a setting or a kind, change `profiles.h` (the structs), `read_book` and `write_item` or `write_settings` (reading and writing), the `header` comment, `check_count` if it takes paths, and the README table. Then add a check that the new line survives a round trip.

## How windows are found and placed

`tiling.cpp` starts each program from the user folder, never from Crammy's folder; a program that inherits Crammy's folder as its working folder stops that folder from being renamed.

| Program | Started with | Its window is found by |
|---|---|---|
| PdfCraft | `pdfcraft.exe --new-window --mode read "a.pdf" "b.pdf"` (from `pdfcraft_command`; several files give tabs in one window); a window Next opens for lecture decks also gets `--control <file>` | the process ID of the program started |
| Acrobat | `Acrobat.exe /n /A "view=FitH" "file.pdf"` (from `acrobat_command`; `/n` forces a new window, `view=FitH` fits the page width) | a title containing the file name and "Adobe Acrobat"; `/n` hands off to another process, so the ID cannot be used |
| Notepad++ | `notepad++.exe -multiInst -nosession "file.txt"` (a window of its own, from `notepad_command`; a reopened session adds its other files after the first) | a title containing any of its file names and "Notepad++" |
| File Explorer | `explorer.exe "folder"` | window class `CabinetWClass` and the folder name in the title |

PdfCraft's `--mode read` opens Read mode for that window, over its own default. With no `--zoom`, PdfCraft uses its default zoom, fit to width; its `--zoom` only takes a number, and an unknown option such as `--fit` is ignored with a warning in its log. A PdfCraft window started with `--control <file>` can be asked about its state with `pdfcraft-cli ui --control <file> state`, which is how these options were checked.

The program paths are in `program_file` in `tiling.cpp`. `window_targets` works out each window's rectangle without opening anything: each side is split into equal rows for the windows on it that go to the same desktop, the line between left and right is at the setup's `split`, and a window with `has_frame` (a reopened one) gets its saved frame. Windows 11 draws invisible borders around windows, so sizes use the visible frame from `DwmGetWindowAttribute(DWMWA_EXTENDED_FRAME_BOUNDS)`.

`open_windows_recorded` does the same as `open_windows` and also returns an `opened_window` for each file: the window that shows it, the program and the side. Next keeps these to know which windows it opened.

Programs often restore their own saved size a moment after appearing, so `open_windows` keeps moving each window back until all of them stay put for four checks in a row (15 seconds at most).

A new program's window can appear a moment before Explorer puts it on any desktop. Moving it then fails (`move_window_to_desktop` says it is "not on any desktop yet"), so the move is tried again on every pass until it works, and is reported only if it never does. Before this retry, a reopened window sometimes stayed on the desktop showing.

A window for the desktop showing with a side to itself is then snapped by pressing Win+Left or Win+Right, which makes Windows treat it as snapped. Before pressing keys, `bring_to_front` presses F24 (a key no keyboard has) because Windows only lets the program that sent the last key press bring a window forward. A window that does not end up in place is moved there directly. Two or more windows on one side are only moved, never snapped: snapping to quarters was tried and did not work reliably with PdfCraft. Windows only snaps to halves, so a setup with a `split` other than 50, a window going to another desktop, and a reopened window are all moved straight to their rectangle, with no keys pressed.

`crammy.manifest` makes the program aware of per-monitor DPI, so all coordinates are real pixels. Do not remove it.

## Desktops

**Reading and switching.** Windows keeps the desktops in the registry under `HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\VirtualDesktops`: `VirtualDesktopIDs` lists them in order, `CurrentVirtualDesktop` is the one showing, and each name is under `Desktops\{id}\Name`. An unnamed desktop is called "Desktop 2" and so on. `desktop_names` and `current_desktop_index` read this. `switch_to_desktop` jumps straight to the desktop with `SwitchDesktop` on Explorer's internal interface (see Creating and naming, below), so going from Main to 198 is one move. If that interface cannot be reached, it presses Ctrl+Win+Left or Right as many times as needed instead. Either way it waits until the registry shows the new desktop. **Moving windows.** `move_window_to_desktop` moves any program's window with `MoveViewToDesktop` on the same internal interface, after getting the window's view from Explorer's undocumented `IApplicationViewCollection` (IID 1841C6D7-4F9D-42C0-AF41-8747538F10E5, slot 3 `GetViewForHwnd`, from the same `VirtualDesktop11-24H2.cs`). It checks the result with the documented `GetWindowDesktopId`. These calls can change between Windows updates, and `tests/test_desktop_setups.cpp` fails when they do.

**Windows on each desktop.** `windows_by_desktop` lists the top-level windows that are visible and have a title. Windows with an owner are left out. For each one it asks the documented `IVirtualDesktopManager::GetWindowDesktopId` and matches the answer to the ids in the registry. Windows whose desktop cannot be found are left out. Explorer does not track some windows on any desktop, such as tool windows (`WS_EX_TOOLWINDOW`) and windows that cannot be activated (`WS_EX_NOACTIVATE`), so these never appear. A test that needs a window on a desktop must make an ordinary application window.

**Creating a desktop.** There is no documented call for this. `create_desktop` reaches Explorer's `IVirtualDesktopManagerInternal` through the immersive shell service, using the interface for Windows 11 24H2 and later, which is build 26200 on this machine. The interface is called by slot number, so a wrong order would call the wrong method. The order was taken from MScholtes' `VirtualDesktop11-24H2.cs`, version 1.21 (github.com/MScholtes/VirtualDesktop), and the comment above `desktop_manager_internal` in `desktops.cpp` lists it. After a Windows update, check that list against the new build before trusting the calls. Crammy calls `CreateDesktop` and then `SetDesktopName`.

If the interface cannot be reached, or `CreateDesktop` fails, Crammy presses Ctrl+Win+D instead. That makes a desktop and switches to it, but leaves it unnamed. If the desktop was made and only the name failed, nothing more is pressed. In both cases `create_desktop` returns true and sets `problem` to a sentence saying the desktop needs its name in Task View, and the panel shows it.

## Notes tabs in Notepad++

Notes are plain `.txt` files opened in Notepad++. Crammy writes each one it opens to `notes opened.txt` (next to the exe, not committed). Before the next setup opens, `close_old_notes` sends messages straight to the Notepad++ window; these are the numbers from Notepad++'s `Notepad_plus_msgs.h`, where `NPPMSG` is `WM_USER + 1000`:

| Message | Use |
|---|---|
| `NPPMSG + 7`, lParam 1 or 2 | number of tabs in the main or second view |
| `NPPMSG + 28`, wParam view, lParam index | switch to a tab |
| `NPPMSG + 38` | save the tab showing |
| `NPPMSG + 48`, lParam 41003 | menu command File > Close |

Each notes file has a Notepad++ window of its own (`-multiInst`), so `close_old_notes` looks at every window of class `Notepad++`. A window whose only file is an old notes file is saved and closed whole. Notepad++'s second view always reports one empty document even while it is hidden, so it is counted only when `NPPMSG + 23` (current document index) for it is not -1.

The title of the Notepad++ window shows the full path of the tab showing, with `*` in front while it has unsaved changes. A tab is closed only after saving cleared the `*`; otherwise it stays open and stays on the list. Tabs not on the list are never touched, and the tab that was showing before is shown again at the end. None of this brings Notepad++ forward or presses keys.

Windows 11 Notepad has no way for another program to save or close a tab, which is why notes moved to Notepad++.

## Saving and reopening a session

`session.cpp` handles the tray menu items **Save and close all** and **Reopen last session**, and `crammy --save-and-close` and `crammy --reopen`. Study windows are every window of `pdfcraft.exe`, every Notepad++ window of class `Notepad++` and every Acrobat window of class `AcrobatSDIWindow`, whoever opened them.

`save_and_close` records each window's program, desktop name and visible frame (for a minimized window, the frame it goes back to), and its files. For Notepad++ it goes through every tab, saves each one that has a file, and lists those files. An empty "new 1" tab is marked saved with Scintilla's `SCI_SETSAVEPOINT` (2014) so closing does not ask about it; a "new 1" tab with text keeps the whole window open, with a message. `SCI_GETLENGTH` (2006) tells the two apart; both are sent to the view's `Scintilla` child and take no pointers, so they work from another process. For PdfCraft and Acrobat the files are the PDFs on the process's command line, read with `NtQueryInformationProcess` class 60. Each window then gets `WM_CLOSE`; one still open after 5 seconds, for example one asking about unsaved changes, is named in a message.

The record goes to `last session.txt` next to the exe (not committed), with `asked = no` at the top and a `[window]` block for each window. When the background Crammy starts and finds `asked = no`, it asks once whether to reopen. Either answer sets `asked = yes`. This happens on any start of the background copy, so restarting it after a build asks too. `reopen_session` turns the record into planned windows with `has_frame` set, drops files that no longer exist, and opens them all at once on their desktops without switching.

## Background mode and the panel

`crammy.exe` takes these options for background mode:

| Option | What it does |
|---|---|
| `--background` | Starts with no window: a tray icon, the keyboard shortcut and a hidden window. Exits at once when a background Crammy is already running |
| `--background --show` | The same, and shows the panel as soon as it has started. When a background Crammy is already running, asks it to show the panel and exits |
| `--panel` | Asks the running background Crammy to show or hide its panel, then exits. When none is running, starts one with `--background --show` and exits |

`--panel` starts the new copy itself with `CreateProcessW` and `DETACHED_PROCESS`, from the folder of `crammy.exe`. The copy then keeps running when whatever ran `--panel` ends, such as a taskbar button. If the start fails, it is tried once more with `CREATE_BREAKAWAY_FROM_JOB` added. An earlier version started it through `explorer.exe`, which can drop the arguments.

**The window and the message.** `run_background` in `background.cpp` makes a hidden top-level window of the class `CrammyBackground`. Anything that wants the panel finds that window and posts it the registered message `CrammyShowPanel` (`show_panel_message()`, from `RegisterWindowMessageW`). With a wparam of 0 the window calls `toggle_panel`, which shows the panel or hides it. `--panel` and the Windhawk mod both send 0 (`--panel` through `ask_running_crammy_to_toggle_panel`). `--background --show` sends 1 through `ask_running_crammy_to_show_panel`, and the window then calls `show_panel`, which opens the panel and leaves an open one as it is. Keep the class name and the message name as they are: the mod has them written in.

**One copy only.** `run_background` first takes the named mutex `CrammyBackgroundLock` and holds it until the process ends. A second copy finds the mutex taken and exits. The mutex covers two launches at the same moment, before either has made its window; `background_is_running` only looks for the window.

**The shortcut.** It is registered with `RegisterHotKey` from the `shortcut` setting, and the default is Ctrl+Alt+C. The setting is read once, when the background copy starts, so a change needs a restart of it. If another program already owns the keys, registering fails without any message and the tray icon still works.

**The tray icon.** A left click shows or hides the panel. A right click opens a menu with Setups, Edit timetable (opens `timetable.txt` in Notepad++), Start at sign-in (ticked when the Startup shortcut exists; choosing it flips `start at sign in` in `settings.txt` and makes or removes the shortcut) and Quit. The icon is added again when Explorer restarts.

**Start at sign-in.** `set_start_at_sign_in` makes or removes `Crammy.lnk` in the user's Startup folder. The background Crammy calls it when it starts, so the shortcut matches the `start at sign in` setting. The shortcut runs `crammy.exe --background` from the folder of the running program. The folder comes from `FOLDERID_Startup`, unless the environment variable `CRAMMY_STARTUP_FOLDER` is set; then that folder is used. The tests set it to a folder in `%TEMP%` so that they never touch the real Startup folder. Do not set it for any other reason.

**The panel.** It is a borderless window, about 300 pixels wide, that opens above the bottom-left corner of the taskbar. Its height comes from what it shows, and it grows upward from the taskbar. It closes when it loses focus or Esc is pressed. `docs/panel-design.md` is the approved design; the owner made the panel smaller and plainer after it was built, and the end of that file says how.

From the top, the panel has these parts. `lay_out_panel` in `panel.cpp` places them. `click` and `over_something` use the same rectangles as the drawing, so a click acts on what is drawn under the mouse.

- The date line, with the date on the left. While the next class has lecture decks open today, a red `next deck` link is at its right end (see Next below). The clock was taken out; the taskbar already shows the time.
- A row for each class today. A row starts with the start time and the course, and then gives the kind as Quest's three-letter code in lower case, such as `lec`. Only the marked row (the class on now, or the next one today) is drawn in red, with a red bar at its left. That row alone also shows the room, and at the right `Nm` for the minutes until it starts or `now` while it is on. A course with no folder in Downloads is grey and does nothing when clicked.
- The message line, under the rows, when `today_section.message` is not empty. A timetable error comes first. Without one, the line shows the first settings error, and with neither it reads `No classes today` on a day with no classes. With no rows, the message is the only line in this part. A timetable error never hides the rows.
- A chip for each virtual desktop, four to a row, with the desktop's name and the icons of up to 3 programs open on it. The chip of the desktop showing has a red outline.
- Plain red text links: `Pomodoro 3h` and `1.5h` on the left, and `Setups…` on the right. A link is underlined while the mouse is over it.

A problem from a click, such as a desktop that could not be made, is drawn under all of this when the panel opens again.

What the panel shows and what a click does are worked out in `panel_model.cpp`, without any window, so that they can be checked in tests. `panel.cpp` only draws that data and passes clicks to it.

Clicking a class goes to the desktop named after the course number (`desktop_name_for`; `150` for ECE 150). If that desktop does not exist, it is created first. The registry can take a moment to list the new desktop, so `open_class` waits until `desktop_names` has one more name, for up to 2 seconds, before it switches. If nothing is open on it, the setup `<Subject>: Lecture` or `<Subject>: Tutorial` is opened there when it exists. Other kinds of class only switch. `plan_class_click` makes this plan.

The Pomodoro links start Youtonomous through `youtonomous_command`, with the long or the short video from the settings. Nothing else starts Youtonomous.

### Next

`docs/next-design.md` is the approved design. The code is in `next_runner.cpp`, and `background.cpp` passes it the messages and timers of the hidden window.

**The line.** `refresh_line` reads the timetable, finds the class with `find_next_class` and makes the text with `line_text`. The text has two rows joined by a newline, so that it fits the weather's spot on the taskbar, which is about 220 pixels wide. The top row is the course and the kind as a word, such as `MATH 117 Tutorial` or `ECE 150 Lecture`. For a class today, the bottom row is the room followed by when it starts, such as `DWE 1515 · 08:30` or `PSE 5353 · in 5m`. For a class on another day, the weekday and start time come before the room, as in `Tue 10:30 · DWE 3516`. A row longer than 24 characters is cut to 23 and an ellipsis. `No classes` is a single row. While `decks today.txt` has decks from today for the class's setup, the name of the highest of them (`deck_label`) takes the whole bottom row. The text goes through `taskbar_title`, which puts the current desktop's name and a tab in front of it, and `-` in front of it all when Next would do nothing: no class found, or a course with no folder in Downloads. The name comes from `desktop_names()[current_desktop_index()]` and is cut to 12 characters, the last of them an ellipsis when it is longer; with an index of -1 the name is empty, and the title is then `background_title` of the line, with no tab. The result becomes the title of the `CrammyBackground` window. `refresh_line` keeps the line it worked out in `last_line`, and sets the title every 10 seconds and again after every Next or next deck. A third timer, every 500 milliseconds, calls `check_desktop`: it reads `current_desktop_index`, which is two small registry reads, and only when the index has changed does it read the names and set the title again from `last_line`. While Windows is switching or making a desktop, the index or its name can be missing for a moment; the name shown before is then kept, and the next tick tries again, so the box does not blink off. The title is set only when it differs from the one already there, so the mod sees no change between switches. The Windhawk mod reads that title.

**The title contract.** The mod depends on this form, so keep it: an optional `-` first, then the desktop name and a tab when there is a desktop, then the two rows with a newline between them. The mod splits at the first tab, so a desktop name with a tab or a newline in it would be split in the wrong place. A title with no tab, such as `-Crammy` at start or `No classes` with no desktop known, shows no desktop box.

**The message.** The taskbar's next button posts the registered message `CrammyNext` (`next_message()`) to the hidden window, and `ask_running_crammy_for_next` does the same from code. The window calls `run_next`. Keep the message name as it is: the mod has it written in.

**What Next does.** It does what a class click in the panel does, through `plan_class_click` and `go_to_class_desktop`: it creates the course's desktop if needed and switches to it. It passes no tiles to `plan_class_click`, so the setup is named even when the desktop already has windows. The setup follows the panel's rule: a lecture opens `<Subject>: Lecture` and a tutorial opens `<Subject>: Tutorial` if there is one. Labs, tests and seminars only switch. If the setup needs ticks, the tick list shows first. Then `plan_setup` works out the windows. When every file in that plan is shown by a window Next opened in this run that still exists (`all_already_open`), Next only switches. Otherwise `open_plan` in `launch.cpp` opens the plan and the windows are added to the list. The list is kept in memory, so after a restart Next opens the slides again. Windows already on the desktop are left alone.

**Busy.** While Next or next deck is opening windows, or while any tick list of Crammy's is open, another Next or next deck is ignored, and the watches wait. A class click in the panel is ignored then too. Presses of Next that were queued in that time are taken off the queue before busy ends (`drop_queued_presses`), so they do not run afterwards. A class click in the panel opens its setup inside `run_as_busy`, so a press of Next during it is dropped in the same way. After the tick list closes, Next and the panel switch to the class's desktop again before opening, in case he moved away.

**Windows Next owns.** Each `opened_window` keeps the process ID of its window. `still_ours` checks that the window exists and still belongs to that process, because Windows can give a closed window's handle to a new window. Next counts a window as open, watches it, or sends it `WM_CLOSE` only when `still_ours` says so, and never closes the window it has just opened.

**Watches.** After Next opens a setup, each `newest` line of it gets a watch on the window that shows its file. Every 2 seconds `check_watches` lists the PDFs in that line's folder with how long ago each one's size changed. It then asks `replacement_for` whether a newer finished file fits the pattern. If one does, it is opened with the same program through `open_window_in_place_of`, which moves the new window onto the old window's frame as it is at that moment. It presses no keys and leaves the focus where it is. Then the old window gets `WM_CLOSE`. This happens only while the desktop that Next opened the setup on is showing. A watch ends when its window is gone or the day changes (`watch_has_ended`). The next Next also ends it, unless that Next only switches to the same setup.

**Lecture decks.** The Setups window, a class click in the panel, `--setup` and Next can each open a `sequence` setup, and the Setups window can be a separate copy of `crammy.exe`. So the decks opened for each setup on a day are kept in `decks today.txt`, which every way of opening reads. For a setup with a `sequence` line, `plan_setup` asks `decks_to_open`: when the setup's record in `decks today.txt` is from today, its decks open again as tabs, in the order they were opened. On a new day the deck after the place in `places.txt` opens. Ticked decks open as ticked, which happens when there is no place yet or `--pick` names them. After the windows open, `save_places` in `launch.cpp` adds the decks opened to the record (`after_opening`) and saves the highest of them as the place. `open_setup` calls it as well, so both files are written the same way whichever way the setup opens. A second open on the same day therefore shows the same decks again, and only next deck moves on. The PdfCraft window for the decks starts with `--control <file>`, with the file in `%TEMP%\crammy\`. The background Crammy deletes old control files there when it starts. `places.txt` is written as `places.txt.tmp` and then moved over the old file with `MoveFileExW`, and `decks today.txt` is written through `decks today.txt.tmp` by the same helper. When the old file cannot be read, or the write fails, it keeps what it had, and the panel says `Could not save the place in places.txt.` or `Could not save today's decks in decks today.txt.`

**next deck.** The panel shows `next deck` while `decks today.txt` has decks from today for the next class's setup, and `last deck` when the highest of them is the last deck. `last deck` is plain text: it gets no hand cursor or underline, and a click on it does nothing and leaves the panel open. A click on `next deck` switches to the class's desktop and opens a new PdfCraft window on the same side, with every deck opened today as tabs in the order they were opened, followed by the deck after the highest of them. Only once that window has appeared do the deck windows this run opened close, and the new deck is added to `decks today.txt` and becomes the place. When no window appears, the panel says so. The line then changes to the deck's name. The page he was on is lost. This is the design's fallback: `pdfcraft-cli ui --control <file>` has an `open` method, but it was not tried, since that needs a running PdfCraft on the owner's screen. If it turns out to add a tab to the window, `run_next_deck` can call it instead of reopening the window.

**The Windhawk mod.** `windhawk\crammy-button.wh.cpp` hides the Widgets button on the taskbar and shows Crammy's line in its place, with a faint `›` next button (size 18, opacity 0.55) to the right of it. Both buttons replace the `ButtonBackground…` and `ButtonBorderBrush…` theme keys in their own resources with a clear brush, so they never turn grey when pointed at or pressed. Every 500 milliseconds a Windows timer (`SetTimer` with no window, set on the taskbar's thread, so its callback `refresh_all_lines` runs there) reads the title of the `CrammyBackground` window; versions before 0.8 used a XAML `DispatcherTimer`, which did not tick in the taskbar's XAML host, so the line only changed when the mod was reloaded. `restore_all` kills the timer. Each placed line keeps ordinary references to the parts the timer changes (the desktop name, the two rows and the next button); before 0.12 these were weak references, and the ones to the two rows came back empty while the rows were still on screen, so the line never changed after it was drawn. `restore_all`, and `add_line` for a line the taskbar has dropped, let go of them. `GetWindowTextW` on another process's window reads the stored title and sends no message, so this is cheap, and the mod changes a text block only when its text differs. A title that starts with `-` is shown without the dash, and the next button is greyed. The rest is split at the first tab: the part before it is the desktop name, drawn as a text block of size 15 in the taskbar's own colour, in a `Border` with a 1-pixel line on its right in grey at low opacity, so it is set apart from the rows without a coloured box. A title with no tab collapses the box. The part after the tab is split at the newline into two text blocks, stacked in a font of size 12. A line with no newline fills the top block, and the bottom one is collapsed. The mod setting `line_width`, 270 effective pixels by default (220 before version 0.5), is the widest the whole line may be, padding included, so that it stays before the Start button. The line is a three-column grid with that `MaxWidth`: the desktop name and `›` keep their own widths, the rows take what is left between them, and a row that does not fit ends in an ellipsis. A width saved in Windhawk by an older version stays until the owner changes it. The hidden window is made with the title `-Crammy`, so next stays greyed until the first line is set. A click on the rows posts `CrammyShowPanel`, and a click on `›` posts `CrammyNext`. When the background window does not exist, the top row reads `Crammy is not running`, and a click on it runs `crammy.exe --panel`. Before posting or running, it calls `AllowSetForegroundWindow`, because Explorer had the click, and Windows only lets the program with the last input give the focus to another program. For the post, it names the process of the `CrammyBackground` window; for `--panel`, it uses `ASFW_ANY`. It does nothing else, so that as little as possible runs inside Explorer. XAML objects may only be touched on the taskbar's own thread, so `run_on_taskbar_thread` hooks that thread and sends it a message; when the caller is already on that thread, as Windhawk sometimes is when it unloads the mod, it runs the work directly. If it cannot reach the thread it logs why, either `No taskbar window in this process` or `Could not hook the taskbar thread, error N`, and on unload also `Could not reach the taskbar thread to remove the line`. In that case the old line stays on the taskbar, frozen, since its timer is gone. From version 0.7, `add_line` looks for a `CrammyLine` that is not in its own `g_lines`, takes it out of the tree with a log line, `Replacing a line left behind by an older load`, and puts its own in place; before 0.7 the new load found the old line by name and added nothing, so the taskbar kept showing a line that never changed. The log shows in Windhawk once Logging is on in the mod's Advanced tab; compiling a new version switches Logging off again. To read the log without the owner's screen, a small program can listen for `OutputDebugString` messages on the `DBWIN_BUFFER` shared memory (with the `DBWIN_BUFFER_READY` and `DBWIN_DATA_READY` events) and keep the lines that mention `crammy`; that is how the timer and weak-reference faults were found. The refresh costs little: the background copy used about 16 ms of processor time every 20 seconds and 33 MB of memory when measured on 2026-11-15. `windhawk\README.md` says how to add it and switch it off. The owner adds it himself.

## Timetable

`timetable.txt` is read by `read_timetable` in `timetable.cpp`. It has one line for each class block:

```
ECE 150 | LEC | MWF | 13:30-14:20 | PSE 5353 | 2026-10-28 to 2026-12-08
```

The six fields are separated by `|`, and spaces around them are ignored.

| Field | Meaning |
|---|---|
| Course | Kept as written, such as `ECE 150` |
| Kind | Quest's code: `LEC`, `TUT`, `LAB`, `TST` or `SEM`. The panel shows the code in lower case. `kind_name` in `subjects.cpp` turns a code into a word, but it is only used to fill `today_row.kind_name`, which the panel does not draw |
| Days | Quest's letters: `M`, `T`, `W`, `Th` and `F`. `MTTh` is Monday, Tuesday and Thursday, and `TTh` is Tuesday and Thursday |
| Times | 24-hour `HH:MM-HH:MM`; the start must be before the end |
| Room | Kept as written; `TBA` is a room like any other |
| Dates | `YYYY-MM-DD to YYYY-MM-DD`, the first not after the last |

Blank lines and lines starting with `#` are ignored. A line that cannot be read is skipped, and an error `line N: cannot read "..."` is kept. The panel shows the first error in one line under the class rows, so a typo does not hide the rest of the day. When the file does not exist, the error is `cannot find <path>`, and the line reads `timetable.txt: cannot find <path>`.

**Which classes meet on a date.** `classes_on` takes a block when the weekday of the date is one of its days and the date is inside its range, both ends included. Blocks at different start times are separate classes, even for the same course and kind: on a lab day with two blocks, both show. When two blocks have the same course, the same kind and the same start time, only the one with the shorter date range is kept. This is how a one-off change replaces the usual block: a block for one Wednesday with another room wins over the block for the whole term. The classes are sorted by start time, then by course.

**Updating it for a new term.**

1. In Quest, open My Class Schedule and choose the list view.
2. For each class, take each row of its meeting information and write one line: the course, the kind (the section's component code), the days, the times, the room, and the first and last dates of that row. Change the times to 24-hour and the dates to `YYYY-MM-DD`. A class with several rows, such as a lecture that stops for reading week, needs a line for each row. A class with no time, such as an online one, is left out.
3. Put the lines in `timetable.txt` in place of the old ones, and update the comment at the top.
4. Check that each course has a folder in Downloads named like it, such as `ECE 150` (spaces and case do not matter), or its row is shown in grey and does nothing. The desktop for a course is named with its course number. For a click to open anything, `profiles.txt` needs the setups `<Subject>: Lecture` or `<Subject>: Tutorial`.
5. Run `.\tests\test.cmd`. The timetable checks read the real file and name real days of the Fall 2026 term, so they will fail for a new term. First correct `tests\specs\timetable.md` to the new term, then change the checks to match it. A check is not edited to make it pass.

Edits to the file apply the next time the panel opens.

## Settings

`settings.txt` is next to `crammy.exe`, in the same `key = value` style as `profiles.txt`. Lines starting with `#` and blank lines are ignored. Key names ignore case, and spaces around the value are trimmed. A missing key keeps its default. An unknown key or a shortcut that cannot be read gives an error `line N: ...` and keeps the default. So does a `start at sign in` value other than `yes` or `no`. The panel shows the first error in its message line, as `settings.txt: line N: ...`, unless the timetable has an error, which comes first. The file may be missing altogether.

| Key | Default | Meaning |
|---|---|---|
| `shortcut` | `Ctrl+Alt+C` | The keys that show the panel. Modifiers are Ctrl, Alt, Shift and Win, and at least one is needed. The key is a letter, a digit, F1 to F24 or Space |
| `youtonomous` | `C:\Code\Youtonomous\build\youtonomous.exe` | The program the Pomodoro links start |
| `pomodoro long` | `C:\Users\Justin\Videos\Youtonomous\74cOUSKXMz0.mp4` | The video for the 3-hour link |
| `pomodoro short` | `C:\Users\Justin\Videos\Youtonomous\sdhh7AYzsTY.mp4` | The video for the 1.5-hour link |
| `start at sign in` | `yes` | `yes` or `no` |

`youtonomous_command` makes the command `"<program>" "<video>"`. When the program or the video is missing, it gives the sentence `Youtonomous was not found at <path>` or `The video was not found at <path>` for the panel to show.

To add a setting, change `crammy_settings`, `default_settings`, `read_settings` and `write_settings` in `settings.cpp`, then this table. Write the spec first (see Testing).

## Testing

The owner's rule is that every behaviour has a spec in `tests\specs`. The tests are written from that spec by someone who has not seen the code, and they are written before the code. They are run once to see them fail, and the code is finished when they all pass.

- A failing test means the code is wrong.
- A test changes only when the spec is corrected first. If a spec turns out to be wrong, change the spec, then the test, then the code.
- The older behaviour has its own spec, `tests\specs\existing.md`, and tests written from it in the same way.

There is one file for each area: `tests\test_<area>.cpp`, with a function `test_<area>()` made of `check( condition, "plain sentence" )` calls from `tests\check.h`. `wmain` in `tests\test_main.cpp` calls every one of them. To add an area, write its spec, write `test_<area>.cpp` from the spec, and call the function from `test_main.cpp`. `tests\test.cmd` already compiles `tests\*.cpp`; a new source file that the tests link against must be added to its list.

The checks never take over the mouse or keyboard and never take screenshots of the owner's screen. He has asked for that not to happen. Windows are made hidden or off screen, and drops are handed to the screen picture in code. When a change cannot be checked this way, add a way to check it, as `close_old_notes` does by taking the window to work on as a parameter.

**What the checks need on this machine.** Some checks use the owner's real setup, so on another machine they can fail for reasons that have nothing to do with a change.

- The profile checks read the real `profiles.txt` in the repo folder, one level above the test program.
- The drop checks use real files under `%USERPROFILE%\Downloads\ECE 190`, such as `1 Lectures\PDF\ECE190-W10-Slides.pdf`.
- The notes check starts a separate, minimized copy of Notepad++ with scratch files in `%TEMP%`, and closes it afterwards. It needs Notepad++ at the path in `program_file`.
- The timetable checks read the real `timetable.txt` and name days of the Fall 2026 term.
- The settings checks make files in `%TEMP%\crammy settings checks`.
- The background checks make a hidden window of the class `CrammyBackground` and set `CRAMMY_STARTUP_FOLDER` to a folder in `%TEMP%`. If a real background Crammy is running, they skip those checks and say so.
- The desktops checks only read. They need an ordinary application window on the current desktop, so the test makes one off screen. They never make any change to the desktops.

**Pictures.** `tests\crammy_test.exe out.bmp` draws a sample setup into `out.bmp`, which is the way to see a change to the screen picture. `tests\crammy_test.exe --panel-picture panel.bmp` draws the panel, with sample classes and desktops, into a `.bmp`. Use it to judge a change to the look of the panel, since the real panel is not shown during the checks.

## Common changes

**Add a program**, such as VS Code: add it to `enum class program` in `tiling.h`, give its path in `program_file`, add how it is started in `open_windows` and how its window is found in `find_window`, add its name to `program_names` in `canvas.cpp`, and decide in `launch.cpp` and `profiles.cpp` how a profile line asks for it (a new kind, or a `with` word like `with acrobat`).

**Add a kind of window line**: see the profile section above, then handle it in `open_setup` (`launch.cpp`) and in `describe` (`canvas.cpp`) so its tile says what it opens. If it can be dropped onto the screen picture, handle it in `add_dropped`.

**Change the look of the screen picture**: `paint` and `describe` in `canvas.cpp`. Run `tests\crammy_test.exe out.bmp` and look at the picture. The owner dislikes styling that looks machine-made, such as pastel cards or paragraphs of instructions on screen. Plain tiles with the real program icons are the current style.

**Add a command line option**: the loop in `wWinMain` in `main.cpp`, and the list at the top of that file and in the README.

**Add a subject or setup**: use the main window, or edit `profiles.txt` and run `crammy.exe --check`.

## Rules for every change

- Variables, fields and parameters are `snake_case`. Function and type names follow the existing code.
- Keep the code plain and readable, in small functions. Comments are few, and each one says why rather than what.
- Commit as Justin Gu `<enderjustin@icloud.com>` with small, focused commits straight to `main`. No `Co-Authored-By` lines, and no files or notes from code-writing tools in the repo.
- Commit messages and documentation use plain words and short sentences. Avoid figures of speech, including saying a file "lives" somewhere; say where it is. Avoid lists of exactly three items in prose; use two, or split the sentence.
- Write the spec and the blind tests first (see Testing), then the code. A failing test is fixed in the code; a spec is corrected before any test that follows it.
- Run `git pull --rebase --autostash` before pushing, and read the push output; a rejected push can look like success if the output is hidden.
- `crammy.exe`, `tests\crammy_test.exe`, `last setup.txt`, `notes opened.txt`, `places.txt` and `decks today.txt` are not committed.
- When a change makes this guide or the README wrong, fix them in the same commit. Remove a Known limits entry as soon as the limit is gone.

## Known limits

- Program paths are fixed in `program_file`. If PdfCraft, Acrobat or Notepad++ moves, Crammy will report that no window appeared.
- A saved session knows only the PDFs a PdfCraft or Acrobat window was started with. A PDF opened later in another tab of the same window is not remembered.
- Save and close all closes study windows the owner opened by hand too. Never run it on his real windows without his say-so.
- `newest` uses the modified date, so re-saving an old PDF makes it the newest.
- When a separate copy of `crammy.exe` opens a setup with decks, the background Crammy does not learn of its windows, so a later next deck leaves the old deck window open under the new one.
- Only one monitor is handled: everything uses the primary monitor's work area, and the panel assumes one as well.
- `create_desktop` and the Windhawk mod are not covered by tests. They change the real desktops or the real taskbar, which a hidden test cannot do, so they were tried by hand. Try them by hand again after a Windows update.
- `create_desktop` depends on the slot order of an undocumented interface for build 26200. On another build it can call the wrong method. `switch_to_desktop` uses the same interface, through `SwitchDesktop` and `FindDesktop`. The Ctrl+Win+D fallback only runs when the interface cannot be reached or `CreateDesktop` fails.
- The Windhawk mod hooks a function in `Taskbar.View.dll` by its debug symbol, so a Windows update can rename it. Windhawk then reports that the mod failed to load, and the taskbar stays as Windows made it.
- The mod needs Windhawk itself to keep working. If the mod breaks, switch it off in Windhawk to bring the Widgets button back; the shortcut and the tray icon work without it.
- The mod has the path to `crammy.exe` as a setting, and uses it to start Crammy when the background window is missing. Change the setting when Crammy moves.
- Next, next deck and a slide replacement open their windows on the background thread, so the tray icon, the shortcut and the panel do not answer until the windows are in place, for up to about 17 seconds. Presses of Next made in that time are dropped.
- The panel only knows the windows that Explorer tracks on a desktop (see Desktops). Tool windows and similar are not shown on the tiles.
