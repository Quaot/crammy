# Crammy button

`crammy-button.wh.cpp` is a Windhawk mod for the Windows 11 taskbar. It hides the Widgets button, which shows the weather at the left end of the taskbar. In its place it puts Crammy's line, such as `Mon 08:30 MATH 117 tut · DWE 1515`, with a `▸ next` button after it. Both are drawn in the taskbar's own font and text colour.

Crammy keeps the line as the title of its hidden `CrammyBackground` window, and the mod reads that title every two seconds. A title that starts with `-` means Next has nothing to open for that class, so the mod shows the line without the `-` and greys the next button.

A click on the line opens the Crammy panel, because the mod posts the `CrammyShowPanel` message to the `CrammyBackground` window. A click on `▸ next` posts the `CrammyNext` message, and Crammy then goes to the class's desktop and opens its slides. When Crammy is not running, the line reads `Crammy is not running` and the next button is greyed, while a click on the line starts `crammy.exe --panel`. The mod does nothing more than that.

## Adding it

1. Open Windhawk and press Create a new mod.
2. Delete the sample code, then paste in the whole of `crammy-button.wh.cpp`.
3. Press Compile Mod. When it compiles, the mod is switched on, and the line appears on the taskbar.
4. Press Exit Editing Mode.

To move from an older version, open the mod in Windhawk, press Edit, paste the new `crammy-button.wh.cpp` over the old code, and press Compile Mod again.

The mod has one setting: the path to `crammy.exe`. It starts as `C:\Users\Justin\Desktop\Projects\Crammy\crammy.exe`. Change it if Crammy moves.

## Switching it off

In Windhawk, open the mod and turn off the Enabled switch, or remove the mod. The Crammy line goes away and the Widgets button comes back. Explorer does not need a restart. The Crammy shortcut and the tray icon keep working without the mod.

## How it works

The mod follows the approach of two published mods by m417z. They are "Taskbar height and icon size" (`taskbar-icon-size`) and "Taskbar tray icon spacing and grid" (`taskbar-notification-icon-spacing`). Like them, it hooks a function in `Taskbar.View.dll`, found by its debug symbol. That function is `AugmentedEntryPointButton::UpdateButtonPadding`, which the taskbar calls for the Widgets button. When the mod is switched on while the taskbar is already showing, it reaches the taskbar's XAML tree through `taskbar.dll` the way the tray spacing mod does.

A Windows update can rename these functions. If that happens, Windhawk reports that the mod failed to load, and the taskbar stays as Windows made it.

The line is refreshed by a XAML `DispatcherTimer` on the taskbar's own thread. Windows reads another program's window title from its own copy, so a hung Crammy cannot hold up the taskbar.
