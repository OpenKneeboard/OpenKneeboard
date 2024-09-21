---
parent: Troubleshooting
---

# Window Capture

Capture will work with most apps, but has limitations.

## Dialog boxes and drop-down menus

On apps using older frameworks, dialog boxes and drop-down menus may not be shown. There is no fix or workaround.

## The captured window doesn't update when it's not visible (e.g. when in-game)

Some applications choose not to update when the window is fully covered by other windows, including full-screen games; OpenKneeboard can not change this behavior. You can ask the developer of the application to remove this optimization, or, some apps may have workarounds:

### Chrome, Edge, and other Chromium-based browsers

Change your browser shortcut to add `--disable-backgrounding-occluded-windows` after the `exe`. If there is a quote after the .exe, add a space after the quote then the extra text, otherwise, just add a space then the extra text. Close your browser and restart.

### Discord

Close Discord fully, including from the system tray.

If your Discord shortcut is to Discord's `Update.exe`, change your shortcut to add `--process-start-args=--disable-backgrounding-occluded-windows` after the `exe`, before `--processStart=Discord.exe` after `Update.exe`; If there is a quote after `Update.exe`, add a space after the quote then the extra text, otherwise, just add a space then the extra text.

If your Discord shortcut is directly to `Discord.exe`, treat it [as a Chromium browser](#chrome-edge-and-other-chromium-based-browsers).

These steps *may* work for other Electron-based apps.

### Other apps

You can try making it so that some portion of the app window is always visible on a monitor - and don't run the game full-screen. If this doesn't work or isn't practical for you, contact the developer of the other app for support; you may want to link them to this FAQ entry.

## Cursor

While OpenKneeboard's own cursor will move, it will not move the Windows mouse cursor, as Windows only supports mouse interaction with the active window.

## Mouse/tablet forwarding

If mouse/tablet forwarding does not work for you with a specific app, it is simply incompatible. There is no configuration, no troubleshooting, or anything else you can do to fix it, [unless you are the app developer, and familiar with Win32 and your framework](../faq/third-party-developers.md#why-doesnt-the-mouse-emulation-work-correctly-in-my-app-when-using-window-capture-tabs).

Apps, frameworks, and Windows itself try very hard to make it so mouse movement only affects active window; sometimes, despite OpenKneeboards' best efforts, they succeed.

These issues are very time consuming, and there are too many apps/frameworks for me to investigate them. There is no need to ask, or to open bug reports/feature requests.

### Specific apps with compatibility issues

- Windows Calculator (UWP)
- Map drag-and-drop in LittleNavMap
- DCSTheWay