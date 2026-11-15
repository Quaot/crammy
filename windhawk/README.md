# Crammy button

`crammy-button.wh.cpp` is a Windhawk mod for the Windows 11 taskbar. It hides the Widgets button, which shows the weather at the left end of the taskbar. In its place it puts Crammy's line in two small rows, as the weather showed its temperature over its condition. The top row names the next class, such as `MATH 117 Tutorial`, and the bottom row, a little dimmer, says where and when, such as `Mon 08:30 · DWE 1515`. A `▸` next button sits to the right of the rows, centred on their height. The rows and the button are drawn in the taskbar's own typeface and text colour, with the rows in a smaller size. At the left of the line, before the rows, the name of the virtual desktop showing is written in bold white on a box of PdfCraft's red, such as `150`, so the desktop can be told at a glance.

Crammy keeps the line as the title of its hidden `CrammyBackground` window: the desktop's name, a tab, then the two rows with a newline between them. The mod reads that title every half second, which costs little because reading another program's window title sends that program no message, and it changes the text on the taskbar only when the title has changed. A title that starts with `-` means Next has nothing to open for that class, so the mod shows the line without the `-` and greys the next button. The part before the first tab goes in the red box. A title with no tab shows no box, and a line with no newline, such as `No classes`, is shown as the top row alone. Crammy cuts a desktop name longer than 12 characters to 11 and an ellipsis.

A click on the rows opens the Crammy panel, because the mod posts the `CrammyShowPanel` message to the `CrammyBackground` window. A click on `▸` posts the `CrammyNext` message, and Crammy then goes to the class's desktop and opens its slides. When Crammy is not running, the top row reads `Crammy is not running` with no desktop box and the next button greyed, while a click on the row starts `crammy.exe --panel`. The mod does nothing more than that.

## Adding it

1. Open Windhawk and press Create a new mod.
2. Delete the sample code, then paste in the whole of `crammy-button.wh.cpp`.
3. Press Compile Mod. When it compiles, the mod is switched on, and the line appears on the taskbar.
4. Press Exit Editing Mode.

To move from an older version, open the mod in Windhawk, press Edit, paste the new `crammy-button.wh.cpp` over the old code, and press Compile Mod again. Version 0.5 adds the desktop box and raises the default `line_width` from 220 to 270 so the box fits. Windhawk keeps a `line_width` already saved by an older version, so after the update the setting still reads 220 until you change it to 270 in the mod's Settings tab.

The mod has two settings. The first is the path to `crammy.exe`, which starts as `C:\Users\Justin\Desktop\Projects\Crammy\crammy.exe`; change it if Crammy moves. The second, `line_width`, is the widest the whole line may be, `▸` included, in pixels at 100% scaling. It starts at 270, wide enough for the desktop box and the rows together. The box and the `▸` button always keep their full widths, and the rows get what is left between them. Lower it if the line reaches the Start button, or raise it if the taskbar's buttons are not centred. A row that does not fit ends in an ellipsis.

## Switching it off

In Windhawk, open the mod and turn off the Enabled switch, or remove the mod. The Crammy line goes away and the Widgets button comes back. Explorer does not need a restart. The Crammy shortcut and the tray icon keep working without the mod.

## How it works

The mod follows the approach of two published mods by m417z. They are "Taskbar height and icon size" (`taskbar-icon-size`) and "Taskbar tray icon spacing and grid" (`taskbar-notification-icon-spacing`). Like them, it hooks a function in `Taskbar.View.dll`, found by its debug symbol. That function is `AugmentedEntryPointButton::UpdateButtonPadding`, which the taskbar calls for the Widgets button. When the mod is switched on while the taskbar is already showing, it reaches the taskbar's XAML tree through `taskbar.dll` the way the tray spacing mod does.

A Windows update can rename these functions. If that happens, Windhawk reports that the mod failed to load, and the taskbar stays as Windows made it.

The line is refreshed every 500 milliseconds by a XAML `DispatcherTimer` on the taskbar's own thread. Windows reads another program's window title from its own copy, so a hung Crammy cannot hold up the taskbar.

## When something goes wrong

In Windhawk, open the mod, go to its Advanced tab and switch on Logging. The mod then writes a line whenever it cannot place the line or reach the taskbar, such as `No taskbar window in this process` or `Could not hook the taskbar thread, error 5`. If the line stays on the taskbar after the mod is switched off, or does not come back after it is switched on, switch the mod off and on again and keep those log lines for whoever fixes it.
