# Spec: starting at sign-in with a Task Scheduler task

The tests go in `tests\test_sign_in_task.cpp`, in `void test_sign_in_task();`, made of `check()` calls from `tests\check.h`. `tests\test_main.cpp` calls it after `test_background_log();`. The interfaces are in `src\background.h`.

This replaces the **Start at sign-in** part of `tests\specs\background.md`. Crammy no longer keeps a shortcut in the Startup folder, so `check_start_at_sign_in` and its call go from `tests\test_background.cpp`.

## What it is for

Windows starts the programs in the Startup folder late, after the programs listed under the Run key. On this machine the background copy started about 80 seconds after sign-in, and until then the taskbar line said Crammy was not running. A Task Scheduler task with a sign-in trigger starts it within a few seconds. A user without admin rights can register such a task for their own account, as long as the trigger and the task's user are that account.

## Interface (`src\background.h`)

```cpp
// The name of the sign-in task: L"Crammy", or the value of CRAMMY_SIGN_IN_TASK when that is set.
wstring sign_in_task_name();

// The Task Scheduler XML for a task that runs exe with --background when user signs in.
wstring sign_in_task_xml( const wstring &exe, const wstring &user );

// The account Crammy runs as, in the form COMPUTER\name.
wstring current_account();

// True when a task named sign_in_task_name() is registered in the root folder of Task Scheduler.
bool sign_in_task_exists();

// The XML of the registered task, as Task Scheduler gives it back; "" when there is none.
wstring registered_sign_in_task_xml();

// On: registers the sign-in task for the current account, running crammy.exe from the folder of the
// running program, and removes the old Startup shortcut. Off: removes the task and the shortcut.
bool set_start_at_sign_in( bool on );

// True when the task or the old Startup shortcut exists. The tray menu ticks Start at sign-in by it.
bool start_at_sign_in_is_on();
```

`startup_shortcut_path()` stays as it is (with `CRAMMY_STARTUP_FOLDER`), since Crammy still removes the old shortcut.

## 1. The task's XML (`sign_in_task_xml`)

Called with `C:\Tools\crammy.exe` and `PC\Ann`. A test reads the result as text and checks it contains these, each as written here:

- It starts with `<?xml version="1.0" encoding="UTF-16"?>`.
- `<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">` and the closing `</Task>`.
- `<LogonTrigger>`, holding `<Enabled>true</Enabled>` and `<UserId>PC\Ann</UserId>`. There is no `<Delay>` anywhere in the text.
- `<LogonType>InteractiveToken</LogonType>` and `<RunLevel>LeastPrivilege</RunLevel>`. `<UserId>PC\Ann</UserId>` appears twice in all, once in the trigger and once in the principal.
- `<MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>`.
- `<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>` and `<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>`, since this is a laptop and Task Scheduler's defaults would stop it on battery.
- `<ExecutionTimeLimit>PT0S</ExecutionTimeLimit>`, since the default limit of three days would end Crammy.
- `<Priority>5</Priority>`, since the default of 7 runs below normal.
- `<Command>C:\Tools\crammy.exe</Command>`, `<Arguments>--background</Arguments>` and `<WorkingDirectory>C:\Tools</WorkingDirectory>`.
- `<Enabled>true</Enabled>` appears at least twice (the trigger and the settings).

Text from the path and the account is escaped: `&` becomes `&amp;`, `<` becomes `&lt;` and `>` becomes `&gt;`. Called with `C:\A&B <x>\crammy.exe` and `PC\Ann`, the text contains `<Command>C:\A&amp;B &lt;x&gt;\crammy.exe</Command>` and `<WorkingDirectory>C:\A&amp;B &lt;x&gt;</WorkingDirectory>`, and does not contain `A&B`.

## 2. The name and the account

- With `CRAMMY_SIGN_IN_TASK` unset, `sign_in_task_name()` is `Crammy`.
- With it set to `Crammy check`, `sign_in_task_name()` is `Crammy check`.
- `current_account()` is not empty, contains one backslash that is neither its first nor its last character, and the part after the backslash equals, ignoring case, what `GetUserNameW` gives.

## 3. Turning it on and off

These register a real task, so the test sets `CRAMMY_SIGN_IN_TASK` to `Crammy check` and `CRAMMY_STARTUP_FOLDER` to `%TEMP%\crammy sign-in check` (made at the start) before any of them, and never touches the task named `Crammy`. The test calls `CoInitialize( nullptr )` at its start and `CoUninitialize()` at its end, when the first succeeded.

- At the start the test calls `set_start_at_sign_in( false )`, in case an earlier run left the task behind. It returns true. Then `sign_in_task_exists()` is false, `registered_sign_in_task_xml()` is empty and `start_at_sign_in_is_on()` is false.
- The test writes a small file named `Crammy.lnk` into the temporary Startup folder, standing in for an old shortcut. `start_at_sign_in_is_on()` is now true.
- `set_start_at_sign_in( true )` returns true. Then:
  - `sign_in_task_exists()` and `start_at_sign_in_is_on()` are true.
  - The `Crammy.lnk` file in the temporary folder is gone.
  - `registered_sign_in_task_xml()` is not empty. It contains, ignoring case, the running test program's folder followed by `\crammy.exe`, and it contains `--background`, `LogonTrigger` and `IgnoreNew`. It also contains `current_account()`, ignoring case. (Task Scheduler rewrites the XML it gives back, so the test looks only for these pieces, not for whole elements.)
- `set_start_at_sign_in( true )` a second time returns true, and the task still exists.
- `set_start_at_sign_in( false )` returns true. Then `sign_in_task_exists()` and `start_at_sign_in_is_on()` are false, and `registered_sign_in_task_xml()` is empty.
- `set_start_at_sign_in( false )` again, with no task, also returns true.
- At the end the test unsets both variables, removes the temporary folder, and checks that `sign_in_task_name()` is `Crammy` again.

## What the background copy does, checked by the owner

- At every start the background copy calls `set_start_at_sign_in` with the `start at sign in` setting, so an old Startup shortcut is replaced by the task the first time the new build runs.
- If registering the task fails, Crammy makes the Startup shortcut as before, so it still starts at sign-in, only later. `set_start_at_sign_in( true )` then returns whether the shortcut was made.
- The tray menu's Start at sign-in is ticked by `start_at_sign_in_is_on()`, and choosing it turns both off or turns the task on.
