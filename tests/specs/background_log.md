# Spec: keeping the background copy alive, and logging why it stops

The tests go in `tests\test_background_log.cpp`, in `void test_background_log();`, made of `check()` calls from `tests\check.h`. `tests\test_main.cpp` calls it after `test_background();`. The interfaces are in `src\background.h`. Temporary files go under `%TEMP%\crammy log checks`, made at the start and deleted at the end. No test writes the real `background log.txt`, and no window a test makes is shown.

## What it is for

After a restart the taskbar line said Crammy was not running, though the background copy had started at sign-in. Windows recorded no crash, so either something asked its hidden window to close, or the program ended without a crash report. The hidden window now ignores requests from other programs to close, and the background copy writes a short log of when it started and why it stopped.

## 1. The hidden window ignores WM_CLOSE

`background_window_proc` is the hidden window's procedure.

- A test registers its own window class, `CrammyLogCheck`, with `background_window_proc` as its procedure, and makes a hidden top-level window of that class (never shown, at x = -32000, y = -32000).
- `SendMessageW( window, WM_CLOSE, 0, 0 )` leaves the window in place: `IsWindow( window )` is still true.
- `SendMessageW( window, WM_ENDSESSION, TRUE, 0 )` returns 0 and leaves the window in place too.
- The test then calls `DestroyWindow( window )`, after which `IsWindow( window )` is false. Destroying the window posts a quit message to the test's thread, so the test removes it straight away with `PeekMessageW( &message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE )`, which must find one.

Quit in the tray menu destroys the window directly, so it still works.

## 2. Log lines (`log_line`, `crash_text`)

- `log_line` with 2026-11-05 08:01:09 and the text `started` gives `2026-11-05 08:01:09  started`: the date and time with leading zeros and two spaces before the text. The milliseconds and the weekday in the `SYSTEMTIME` are ignored.
- `log_line` with 2026-12-31 23:59:59 and an empty text gives `2026-12-31 23:59:59  `.
- `crash_text( 0xC0000005, 0x1A2B3 )` gives `crashed with exception 0xC0000005 at crammy.exe+0x1A2B3`. Hexadecimal digits are upper case, with no leading zeros beyond what the number needs. `crash_text( 0xE06D7363, 0 )` gives `crashed with exception 0xE06D7363 at crammy.exe+0x0`.

## 3. Appending (`append_log`)

The path is a file in the temporary folder.

- With no file there, `append_log( path, L"one", 3 )` makes the file, and its text is `one\r\n`.
- After `two` and `three` are appended the same way, the text is `one\r\ntwo\r\nthree\r\n`.
- After `four` is appended with keep 3, the text is `two\r\nthree\r\nfour\r\n`: the oldest line goes.
- With keep 1, appending `five` leaves `five\r\n`.
- A path in a folder that does not exist does nothing and does not crash.

## 4. The real log (`background_log_path`)

`background_log_path()` is `background log.txt` in the folder of the running program. In the tests that is the `tests` folder, so the test only compares the path and never writes to it.

## What the background copy logs

These are not tested, since they need the real background copy:

- `started` when it starts, with its process ID.
- `stopped: Quit was chosen` when Quit is chosen in the tray menu.
- `ignored a request to close` when another program sends WM_CLOSE.
- `Windows is ending the session` on WM_ENDSESSION.
- `stopped` when the hidden window is destroyed for any reason.
- `crashed with exception ...` from an unhandled exception filter, and `ended by an uncaught C++ exception: <what>` from a terminate handler.
