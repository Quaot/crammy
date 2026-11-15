# Crammy

Crammy opens the files for a study session and puts each window in its place. A lecture setup can open this week's slides on the left and the summary sheet in Acrobat on the right, on that subject's own desktop.

## Use

Run `crammy.exe`. Setups are listed on the left and the middle shows a picture of your screen. Press Open, or double-click a setup, to open it. The last setup you opened is selected next time.

To build a setup, press New setup, then drag PDFs or folders from File Explorer onto the picture. Dropping on the left or right third fills that half of the screen; the middle fills the whole screen. Tiles can be dragged from one side to the other. Right-click a tile to open it in Acrobat, or to make it always open the newest file like it, which suits weekly slides. A folder becomes a list you tick from each time you open the setup. Every change is saved straight away.

Edit as text opens `profiles.txt` in a small editor for anything the picture does not cover.

Notes open in Notepad++, in a window of their own, so each desktop gets its own notes. Each time a setup opens, Crammy saves and closes the notes it opened before, using commands Notepad++ accepts from other programs, so nothing comes to the front and no keys are pressed. A notes window with nothing else in it closes whole. Files you opened yourself stay open.

It also runs from a terminal:

```
crammy --setup "ECE 150: Exam" --pick F24-F
crammy --setup "Math 115: Exam" --pick F23-F F23-M --separate
crammy --setup "Math 115: Exam" --pick Mock-1113 --setup "Math 117: Exam" --pick Mock-1113
crammy --list
crammy --list "ECE 150: Exam"
crammy --check
crammy --edit
```

`--pick` takes the PDF with that exact name, or else the newest one whose name contains the text. It also works on a `sequence` line, to open a chosen deck in place of the next one. `--separate` gives each picked PDF its own window. `--setup` can be given more than once, each with its own `--pick`: every setup opens on its own desktop in one go, and the desktop of the first one shows at the end.

## The panel

The panel is a window about 300 pixels wide. It opens above the bottom-left corner of the taskbar and grows upward to fit what it shows. The top line has the date. During an ECE 150 lecture with decks open, a `next deck` link sits at its right end.

Under the date is a row for each of today's classes from the timetable. A row starts with the start time and the course. After them comes the kind as a lower-case code from Quest, such as `lec` or `tut`. Only the class on now, or the next one today, is drawn in red. That row also shows the room, and at the right the minutes until it starts, such as `25m`, or `now` while it is on. A course with no folder in Downloads is grey. A mistake in the timetable or the settings is shown in one line under the classes.

Below the classes is a chip for each virtual desktop, four to a row, with its name and the icons of up to 3 programs open on it. The chip of the desktop showing has a red outline. At the bottom are plain red text links. `Pomodoro 3h` and `1.5h` start Youtonomous on the long or the short video. `Setups…` brings up the main window.

Clicking a class goes to the desktop named after its course number, and makes that desktop if it is missing. When nothing is open there yet, the subject's setup opens on it. Clicking a chip switches to that desktop.

Open the panel with Ctrl+Alt+C or with the Crammy icon in the tray. A click on Crammy's line on the taskbar opens it as well. That line comes from a Windhawk mod (see `windhawk\README.md`), and the shortcut and the tray icon work without it.

These work while Crammy runs in the background. Start it with `crammy --background`, or choose Start at sign-in in the tray menu to make a shortcut in the Startup folder that does so each time you sign in. `crammy --panel` shows the panel and starts the background copy first when it is not running.

The timetable is `timetable.txt` and the settings are `settings.txt`, both next to the exe. The settings file may be missing; `docs/guide.md` lists its keys and says how to copy a new term's classes from Quest into the timetable.

## Next and the taskbar line

With the Windhawk mod switched on, the taskbar shows the next class where the weather was, in two small rows like the weather's. The top row names the class, such as `ECE 150 Lecture`, and the row under it says where and when, such as `PSE 5353 · in 5m`. For a class on another day, the bottom row starts with its weekday and start time, as in `Tue 10:30 · DWE 3516`. A row longer than 24 characters is cut short with an ellipsis. The line names the class on now, or else the next one today. After the last class of the day, it shows the first class of the next day with classes. Crammy works the line out again every 10 seconds.

At the left of the line, the name of the virtual desktop you are on, such as `150` or `Main`, is written a little larger, with a thin line after it. A name longer than 12 characters ends in an ellipsis. Crammy checks the desktop every half second, so the name changes soon after you switch. With the mod's width setting saved from an older version, raise `line_width` to 270 in Windhawk if the rows look short.

Pressing `▸`, to the right of the two rows, goes to that class's desktop and opens its setup, as a class click in the panel does. When every file of the setup is already showing in windows Next opened, it only switches. For a course with no folder in Downloads, the button is greyed.

After Next opens a setup, Crammy watches the folder of every `newest` line until midnight or the next press of Next. A newer download that fits the pattern opens over the old slides once its size has stopped changing, and the window with the old slides closes. This happens only while that desktop is showing, and Crammy closes only windows it opened itself.

The ECE 150 lecture setup uses a `sequence` line. Next opens the deck after the place saved in `places.txt`, and shows the tick list when there is no place yet. Opening the setup any other way saves the place as well, as the highest deck opened. Every way of opening also writes the decks it opened to `decks today.txt`, next to `places.txt`. A second open on the same day shows the same decks again, and only next deck moves on. While those decks are open, the deck's name takes the whole bottom row, in place of the room and time.

The next deck is reached from the panel only. Its `next deck` link reopens the PdfCraft window with today's decks and the following one as tabs, and moves the place on. At the last deck the link reads `last deck` and does nothing.

## Before shutting down

**Save and close all**, in the tray icon's menu, saves every file open in Notepad++, writes down every PdfCraft, Notepad++ and Acrobat window (its files, its desktop and where it sat on the screen) in `last session.txt`, and closes them all. It covers windows you opened yourself as well as Crammy's. A window it cannot close safely stays open and is named in the message afterwards, such as a Notepad++ tab with unsaved text and no file.

At the next sign-in, Crammy asks once whether to reopen them. Each window goes back to its own desktop and its old place without the desktops switching. **Reopen last session** in the same menu does it again later. From a terminal, `crammy --save-and-close` and `crammy --reopen` do the same.

## Profiles

`profiles.txt` sits next to the exe. A profile names a root folder. Its setups say what opens on each side.

```
[ECE 150]
root = Downloads\ECE 150

[ECE 150: Exam]
left = notes
right = pick "6 Exams" "2 Practice"

[ECE 150: Lecture and lab]
left = tabs "1 Lectures\PDF\*Week 10*"
right = pdf "4 Labs\Lab 5.pdf"
right = folder "4 Labs"
```

A side is `left` or `right`, or `full` for the whole screen. When several windows share a side they are stacked from top to bottom in the order written. A window with a side to itself is snapped the way Win+Left snaps it, so dragging the line between two snapped windows resizes both.

| Line | Opens |
|---|---|
| `notes` | Notepad++ with `<first PDF> - my answers (date).txt`, or the newest file of that name |
| `notes "file.txt"` | Notepad++ with that file, made if missing |
| `pdf "file.pdf"` | one PdfCraft window |
| `newest "L*.pdf"` | the newest PDF whose name fits the pattern |
| `tabs "a.pdf" "b.pdf"` | one PdfCraft window with a tab for each PDF |
| `pick "folder" "file"` | the PDFs you tick at launch |
| `sequence "folder"` | one PdfCraft window with the next numbered lecture deck after the one last opened; before any was opened, the decks to tick |
| `folder "folder"` | File Explorer |

Ending a PDF line with `with acrobat` opens it in Adobe Acrobat in place of PdfCraft. Paths start from the root unless they are absolute. `*` matches any text in a file name, and variables such as `%USERPROFILE%` are expanded. Answer files go in `2 Practice` when the course has one. Set `notes folder = ...` to change that.

`desktop = 150` puts the setup's windows on the virtual desktop named 150, so each subject can keep its own desktop. The windows open on the desktop showing and move there as soon as they appear, so nothing switches while they open. Once they are in place, Crammy shows that desktop in one move. It reads the desktop names from the registry.

`split = 75` gives the left side 75% of the screen width and the right side the rest, like the 75/25 snap layout in Windows. It can go on a subject or on one setup. Without it the sides are halves. The exam setups use it to keep the paper wide on the left with the notes in a narrow strip on the right.

A setup starts with every line of its profile, so lines shared by all setups can go under the profile heading.

## Build

The build needs MSYS2 with the UCRT64 g++. Run `build.cmd`. PdfCraft is expected at `C:\Program Files\PdfCraft`.

`tests\test.cmd` builds and runs the checks. They drop files on the picture in a hidden window, so nothing appears on screen.

`docs/guide.md` explains how Crammy works and how to change it.
