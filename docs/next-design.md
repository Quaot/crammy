# Design: Next, and the Crammy line on the taskbar

Written 2026-11-14 for the owner to approve before anything is built. It builds on `docs/panel-design.md`.

## What it is for

The panel as built shows a clock and a list of names, which the owner can read off the taskbar already. What he wants is to get to the next class's slides in one press, and to see what is next without opening anything. So the weather spot on the taskbar shows one line about the next class, with a Next button beside it. Pressing Next goes to that subject's desktop and opens its slides.

## Goals

- A line of text on the taskbar, where the weather is now, always visible: the next class, its room and when it starts, such as `ECE 150 lec · PSE 5353 · in 5m`. The one-row form was later replaced by two rows; see **Two rows on the taskbar** under Settled after review.
- A **▸ next** button beside the line. One press goes to the class's desktop (in one jump, as switching does now) and opens its slides.
- When newer slides are downloaded after Next has opened old ones, the new file takes the old one's place without a press.
- For ECE 150, whose slides are numbered sections, Next opens the section after the last one opened, and a **next deck** link adds the following section as a tab in the same PdfCraft window. Crammy remembers the place between lectures.
- The clock goes from the panel.

## Not part of this

- The flatter taskbar. It is the next design, and it leaves room for this line.
- Downloading slides. The owner downloads them; Crammy only notices new files.
- Upcoming tests or due dates.

## What would make it a failure even if it worked

- A taskbar that crashes or flickers. The code inside Explorer stays as small as the button mod is now.
- Opening the wrong slides without saying so. When Crammy cannot tell which file is right, it asks with the tick list, as setups with `pick` do now.
- Focus or desktops changing when the owner did not press anything. Replacing old slides with new ones happens only on the desktop Next opened them on, and only to windows Crammy opened.

## Approaches considered

1. **Chosen by the owner:** Crammy works out the line, and the Windhawk mod only shows it. Explorer runs nothing but a text label and two clicks.
2. Crammy draws its own window over the taskbar, with no mod. It needs nothing inside Explorer, but the window flickers or hides when the taskbar redraws, and it would fight the taskbar redesign.
3. The mod does everything itself. It needs no Crammy in the background, but all the logic would run inside Explorer, so a bug crashes the taskbar.

## How it works

### Which class is "next"

The same class the panel marks now: the one on now, or else the next one today. After the last class of the day, the line shows the first class of the next day with classes, with its weekday and start time, such as `Mon 08:30 MATH 117 tut · DWE 1515` (my call; an empty taskbar spot says nothing useful). With no classes in the timetable at all, the line reads `No classes`. The one-row form was later replaced by two rows; see **Two rows on the taskbar** under Settled after review.

Classes for courses without a folder (GENE 119, MTHEL 99) still show on the line. They have no desktop or slides, so for them the **▸ next** button is greyed and does nothing (my call).

### The line

- Text: course, the kind as three letters (`lec`, `tut`, `lab`, `tst`, `sem`), the room, and `now` or `in Nm` when it starts within 60 minutes, or its start time (`13:30`) otherwise. During ECE 150 lectures, once a deck is open, the deck name follows instead of the room: `ECE 150 lec · 3.02 Linear search`. The one-row form was later replaced by two rows; see **Two rows on the taskbar** under Settled after review.
- Crammy recomputes the line every 10 seconds and after every Next or next deck, and keeps it as the title of its hidden `CrammyBackground` window.
- The mod reads that title every two seconds (my call: a timer that reads a window title is the smallest thing that can run in Explorer) and shows it in the taskbar's own font and text colour. No red, cream or engraving on the taskbar itself; the taskbar redesign decides its look.

### The mod

`windhawk/crammy-button.wh.cpp` changes from an icon button to: the line as a text button, then a **▸ next** button. Clicking the text posts `CrammyShowPanel`, as the icon does now. Clicking **▸ next** posts a new registered message, `CrammyNext`. When `CrammyBackground` does not exist, the line reads `Crammy is not running` and a click starts `crammy.exe --panel`, as now. The owner adds the new version to Windhawk himself.

### Next

Next does what a class click in the panel does now (`plan_class_click`): it finds or creates the subject's desktop, switches to it in one jump, and opens the setup for that kind of class. The one change is that it opens the setup even if windows are already open there, because the owner pressed Next to get the slides (my call: a second press while the slides are already showing brings nothing new, so when every file the setup would open is already open, Next only switches).

Ctrl+Alt+C and the panel stay as they are, minus the clock. The panel header keeps the date.

### Newer slides replace old ones

After Next opens a setup with `newest` lines, Crammy watches the folder of each `newest` pattern until the next Next press or the end of the day, whichever comes first. When a file matching the pattern appears that is newer than the one open, Crammy opens it in the same snap position with the same program, and closes the window it opened for the old file. Windows Crammy did not open are never touched. Files still downloading are skipped until their size stops changing for two seconds (my call; a browser writes partial files under the final name for some downloads).

### ECE 150: decks in order

- A new item kind in `profiles.txt`: `sequence "folder"`. It lists the PDFs in the folder whose names start with a section number, `<major>.<minor>`, such as `3.01` or `7.5`, sorted by major then minor as whole numbers. Files without a section number (`Using replit.pdf`, `assignment1.pdf`) are left out.
- `[ECE 150: Lecture]` changes from `full = pick "1 Lectures\PDFs"` to `full = sequence "1 Lectures\PDFs"`.
- Next opens the section after the last one reached. With no saved place, it shows the tick list of the sequence, and the deck he ticks becomes the place.
- **next deck** shows in the panel during a sequence setup's class, and the line shows the deck name. Each press adds the following section as a tab in the PdfCraft window Next opened, and moves the place on. At the last section it says `last deck` and does nothing.
- Places are kept in `places.txt` next to `crammy.exe`, one line per setup: `ECE 150: Lecture = 3.02 Linear search.pdf`.
- Whether PdfCraft can add a tab to a window that is already open is not known yet. The builder finds out from PdfCraft's command line, without the owner's screen. If it cannot, next deck closes that PdfCraft window and reopens it in the same place with all the decks of this class as tabs.

### Style

The panel keeps the PdfCraft look. The line on the taskbar uses the taskbar's own font and colours, so that it fits whatever the taskbar redesign makes.

## Testing

As before: a spec for each behaviour, blind tests written from the spec alone, then the code. The tests cover:

- choosing the class for the line, including after the last class and on days with none;
- the text of the line, including `now`, `in Nm`, a start time, a weekday, and a deck name;
- sorting a sequence folder and skipping unnumbered files, with the real 150 names;
- the next section after a saved place, the last deck, and reading and writing `places.txt`;
- deciding that a new file replaces an open one: newer, matching the pattern, and finished downloading;
- `sequence` lines surviving a round trip through `profiles.txt`.

Switching desktops, closing and opening real windows, and the mod are checked by the owner, once he agrees.

## Settled after review

A builder read this design and asked about the cases below. These are my calls; the owner can change any of them.

- **Which setup Next opens.** The rule from the panel stays: a lecture opens `<Subject>: Lecture`, a tutorial opens `<Subject>: Tutorial` if there is one, and labs, tests and seminars only switch desktops.
- **Which class the line is about.** The rule from the panel stays: a class that is on stays marked until it ends, and then the next one is marked. A kind code other than the five above shows as its first three letters in lower case.
- **"Already open".** Crammy keeps a list of the windows it opened, with the file each one shows, for as long as it runs. Next only switches when every file the setup would open is in a window on that list that still exists. After Crammy restarts the list is empty, so Next opens the slides again. Windows already on the desktop are left where they are.
- **Which file is newer.** The `newest` rule decides, as it does now: the newest file by modified date that matches the pattern. Each `newest` line is watched on its own, so ECE 190's slides and summary are replaced separately. Lines of other kinds are never watched. Watches end at the next Next or at midnight, and a restart drops them.
- **The order of the 150 decks.** The section number is the start of the name, split at the dots and compared part by part as whole numbers, so `3.9` comes between `3.08` and `3.10`. A shorter number comes before the longer ones that start with it, so `2` comes before `2.01` and `2.05` before `2.05.1`. A letter after a number comes after the number alone, so `1.18` is followed by `1.18a` and then `1.18b`, and a letter part such as `1.A` comes after every numbered part of that chapter. `0.Welcome` is section 0, so it comes first. Names that do not start with a number (`A polynomial class.pdf`, `Date class.pdf`, `Using replit.pdf`, `assignment1.pdf`) are left out.
- **Places.** When several decks are ticked with no saved place, the place is the highest of them. If the place names a file that is gone, Crammy uses the highest deck in the order that comes before that name. After the last deck, Next opens the last deck again.
- **Two presses.** A press of Next or next deck while the previous one is still opening windows is ignored.
- **A long line.** The line is cut to 48 characters, with `…` at the end when it is cut. Replaced by **Two rows on the taskbar**: a row is now cut to 24 characters.
- **Next deck in the panel only.** The next deck link is in the panel, and the taskbar line shows the name of the deck reached. A next deck button on the taskbar would be a later change.
- **The next-deck fallback.** If PdfCraft cannot add a tab, the window is reopened with the decks opened for this class today, in order. The page he was on is lost in that case.
- **A second open on the same day.** Every way of opening a `sequence` setup (the Setups window, a class click, `--setup` and Next) reads and writes the decks opened today in `decks today.txt`, next to `places.txt`. A second open on the same day shows the same decks again, and only next deck moves on.

- **Kind as a word, and a wider line.** On 2026-11-14 the owner asked for the kind to be written out, so the top row reads `MATH 117 Tutorial` or `ECE 150 Lecture` (the panel keeps its short codes). The room was cut off at 150 pixels, so the mod's `line_width` now starts at 220. A value already saved in Windhawk stays until it is changed in the mod's settings.
- **Two rows on the taskbar.** Once the owner saw the line on his taskbar, one row ran under the Start and Search buttons, because the weather spot is only about 150 pixels wide. The line is now two short rows, like the weather's temperature over its condition: the class on top, such as `MATH 117 tut`, and where and when below, such as `Mon 08:30 · DWE 1515`, or the deck during a 150 lecture. Each row is cut to 24 characters. Crammy keeps both rows in its window title, separated by a newline, and the mod stacks them in a smaller font with **▸** beside them, inside the space before the Start button.

## Left to the builder

- How the folder watch is done (`ReadDirectoryChangesW` or polling).
- The exact spacing of the line and the button on the taskbar.
- How `next deck` adds a tab, within the fallback above.
