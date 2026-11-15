# Crammy button

`crammy-button.wh.cpp` is a Windhawk mod for the Windows 11 taskbar. It hides the Widgets button, which shows the weather at the left end of the taskbar. In its place it puts Crammy's line in two small rows, as the weather showed its temperature over its condition. The top row names the next class, such as `MATH 117 Tutorial`, and the bottom row says where and when, such as `Mon 08:30 · DWE 1515`. A faint `›` next button sits to the right of the rows, centred on their height. At the left of the line, before the rows, the name of the virtual desktop showing, such as `150`, is written a little larger, with a thin line after it. Everything is drawn in the taskbar's own typeface and text colour, with the rows in a smaller size, and the buttons stay clear when pointed at or pressed.

Crammy keeps the line as the title of its hidden `CrammyBackground` window: the desktop's name, a tab, then the two rows with a newline between them. The mod reads that title every half second, which costs little because reading another program's window title sends that program no message, and it changes the text on the taskbar only when the title has changed. A title that starts with `-` means Next has nothing to open for that class, so the mod shows the line without the `-` and greys the next button. The part before the first tab is the desktop name. A title with no tab shows no name, and a line with no newline, such as `No classes`, is shown as the top row alone. Crammy cuts a desktop name longer than 12 characters to 11 and an ellipsis.

A click on the rows opens the Crammy panel, because the mod posts the `CrammyShowPanel` message to the `CrammyBackground` window. A click on `›` posts the `CrammyNext` message, and Crammy then goes to the class's desktop and opens its slides. When Crammy is not running, the top row reads `Crammy is not running` with no desktop box and the next button greyed, while a click on the row starts `crammy.exe --panel`. The mod does nothing more than that.

## Adding it

1. Open Windhawk and press Create a new mod.
2. Delete the sample code, then paste in the whole of `crammy-button.wh.cpp`.
3. Press Compile Mod. When it compiles, the mod is switched on, and the line appears on the taskbar.
4. Press Exit Editing Mode.

To move from an older version, open the mod in Windhawk, press Edit, paste the new `crammy-button.wh.cpp` over the old code, and press Compile Mod again. Windhawk keeps a `line_width` already saved by an older version, so after an update the setting keeps its old value until you change it in the mod's Settings tab. Since version 0.5 the default is 270, so the desktop name fits beside the rows.

Versions worth knowing: 0.5 added the desktop name, 0.6 made the line plain (no coloured box, one weight, a faint `›`, buttons that never turn grey), 0.7 replaces a frozen line left behind by an older load, 0.8 refreshes with a Windows timer, and 0.12 lets that timer reach the rows, so the line follows desktop switches without a reload.

The mod has two settings. The first is the path to `crammy.exe`, which starts as `C:\Users\Justin\Desktop\Projects\Crammy\crammy.exe`; change it if Crammy moves. The second, `line_width`, is the widest the whole line may be, `›` included, in pixels at 100% scaling. It starts at 270, wide enough for the desktop name and the rows together. The desktop name and the `›` button always keep their full widths, and the rows get what is left between them. Lower it if the line reaches the Start button, or raise it if the taskbar's buttons are not centred. A row that does not fit ends in an ellipsis.

## Switching it off

In Windhawk, open the mod and turn off the Enabled switch, or remove the mod. The Crammy line goes away and the Widgets button comes back. Explorer does not need a restart. The Crammy shortcut and the tray icon keep working without the mod.

## How it works

The mod follows the approach of two published mods by m417z. They are "Taskbar height and icon size" (`taskbar-icon-size`) and "Taskbar tray icon spacing and grid" (`taskbar-notification-icon-spacing`). Like them, it hooks a function in `Taskbar.View.dll`, found by its debug symbol. That function is `AugmentedEntryPointButton::UpdateButtonPadding`, which the taskbar calls for the Widgets button. When the mod is switched on while the taskbar is already showing, it reaches the taskbar's XAML tree through `taskbar.dll` the way the tray spacing mod does.

A Windows update can rename these functions. If that happens, Windhawk reports that the mod failed to load, and the taskbar stays as Windows made it.

The line is refreshed every 500 milliseconds by a Windows timer (`SetTimer` with no window) set on the taskbar's own thread, so its callback runs there and may touch the XAML. A XAML `DispatcherTimer` does not tick in the taskbar's XAML host, which is why versions before 0.8 only changed on a reload. The mod keeps ordinary references to the parts it changes, the desktop name, the two rows and the next button, because weak references to the rows came back empty. Windows reads another program's window title from its own copy, so a hung Crammy cannot hold up the taskbar, and each tick costs a few microseconds.

## When something goes wrong

In Windhawk, open the mod, go to its Advanced tab and switch on Logging. Compiling a new version switches Logging off again, so turn it back on after each update. The mod then writes a line whenever it cannot place the line or reach the taskbar, such as `No taskbar window in this process` or `Could not hook the taskbar thread, error 5`. If the line stays on the taskbar after the mod is switched off, or does not come back after it is switched on, switch the mod off and on again and keep those log lines for whoever fixes it.
