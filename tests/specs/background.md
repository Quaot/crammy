# Spec: background mode and start at sign-in

The tests go in `tests\test_background.cpp`, in `void test_background();`, made of `check()` calls from `tests\check.h`. They make only hidden windows, and they put the startup shortcut in a temporary folder, never the real Startup folder.

## Interface (`src\background.h`)

```cpp
extern const wchar_t *background_class_name;   // L"CrammyBackground"
UINT show_panel_message();                      // RegisterWindowMessageW( L"CrammyShowPanel" )
bool background_is_running();                   // a top-level window of class CrammyBackground exists
bool ask_running_crammy_to_toggle_panel();      // posts show_panel_message() to it; false if there is none
bool set_start_at_sign_in( bool on );           // creates or removes Crammy.lnk in the Startup folder
wstring startup_shortcut_path();                // <Startup folder>\Crammy.lnk
int run_background( bool show_panel_now );      // not called by tests
```

**The Startup folder** is the user's Startup folder (`FOLDERID_Startup`). When the environment variable `CRAMMY_STARTUP_FOLDER` is set, that folder is used in its place, so tests never touch the real one.

## Behaviour

**Sending the toggle message**

- Before the test makes any window of class `CrammyBackground`, `background_is_running()` is false and `ask_running_crammy_to_toggle_panel()` returns false. (The test first checks that no real Crammy is running with `FindWindowW( L"CrammyBackground", nullptr )`. If one is, it skips these checks with a plain line saying why.)
- The test registers a window class named `CrammyBackground` and makes a hidden top-level window of it, whose window procedure counts the messages equal to `show_panel_message()`.
  - `background_is_running()` is then true.
  - `ask_running_crammy_to_toggle_panel()` returns true.
  - After pumping the thread's messages, the count is 1. Asking twice more gives a count of 3.
- `show_panel_message()` gives the same non-zero value on every call.
- After the test destroys the window, `background_is_running()` is false again.

**Start at sign-in**

- With `CRAMMY_STARTUP_FOLDER` set to `%TEMP%\crammy startup check`, `startup_shortcut_path()` is `<that folder>\Crammy.lnk`.
- `set_start_at_sign_in( true )` returns true and creates that file. It is a shortcut (read it with `IShellLinkW`) whose target is the running test program's own folder followed by `\crammy.exe`, and whose arguments are `--background`.
- Calling `set_start_at_sign_in( true )` again returns true and still leaves one file.
- `set_start_at_sign_in( false )` returns true and removes the file. Calling it again when there is no file also returns true.
- The test unsets `CRAMMY_STARTUP_FOLDER` at the end.

## Next (added for the Next design, `docs\next-design.md`)

These are added to `src\background.h`. The tests go in the same `tests\test_background.cpp`, in the same `void test_background();`.

```cpp
UINT next_message();                    // RegisterWindowMessageW( L"CrammyNext" )
bool ask_running_crammy_for_next();     // posts next_message() to the CrammyBackground window; false if there is none

// The title Crammy gives its hidden window: the line, with "-" in front when Next would do nothing.
wstring background_title( const wstring &line, bool next_can_act );

struct recorded_window {
    wstring path;   // the file the window shows
    bool alive;     // the window still exists
};
// True when every file is shown by a recorded window that still exists. Paths are compared
// without regard to case. An empty list of files gives false.
bool all_already_open( const vector<wstring> &files, const vector<recorded_window> &windows );

// True once the day has changed since the watch started, so watches end at midnight.
bool watch_has_ended( calendar_date started, calendar_date today );
```

**Sending Next**

- `next_message()` gives the same non-zero value on every call, and it differs from `show_panel_message()`.
- With no `CrammyBackground` window, `ask_running_crammy_for_next()` returns false.
- With the test's hidden `CrammyBackground` window (the same one as above, its procedure also counting `next_message()`), `ask_running_crammy_for_next()` returns true, and after pumping messages the Next count is 1 while the show-panel count is unchanged. These checks are skipped, with a plain line, when a real Crammy is running, as the toggle checks are.

**The title**

- `background_title( L"ECE 150 lec · PSE 5353 · in 5m", true )` is that line unchanged.
- `background_title( L"GENE 119 sem · STC 0050 · 10:30", false )` is `-GENE 119 sem · STC 0050 · 10:30`.
- `background_title( L"No classes", false )` is `-No classes`.

**Already open**

- Files `{ C:\a\L23.pdf }` with windows `{ (c:\A\l23.PDF, alive) }` → true.
- The same file with that window not alive → false.
- Files `{ C:\a\Slides.pdf, C:\a\Sum.pdf }` with only the Slides window alive → false; with both alive → true.
- An empty list of files → false, whatever the windows.
- Files `{ C:\a\L23.pdf }` with no windows → false.

**Watches end at midnight**

- Started 2026-11-16, today 2026-11-16 → false.
- Started 2026-11-16, today 2026-11-17 → true.
- Started 2026-11-30, today 2026-12-01 → true.

## Window behaviour, checked by the owner

- The hidden window's title is `background_title` of the current line, set at start, every 10 seconds and after every Next or next deck.
- A second Next while the first is still opening windows, or while the tick list is open, is ignored.
