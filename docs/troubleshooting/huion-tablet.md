# Huion Tablets

If you're on v1.3+ and having problems, you might want to [try OpenTabletDriver](https://go.openkneeboard.com/otd-ipc); the rest of this document only applies to Huion's driver.

## Tablet only works properly when OpenKneeboard is the active window

### Symptoms

- Tablet acts a mouse in-game
- Pen buttons don't work in-game
- "Press keys" don't work in-game

### Fixes

1. (v1.3+): make sure that wintab is set to 'invasive' in OpenKneeboard's input settings
2. Make the sure the game (and any launchers, e.g. Skatezilla) are not running elevated/as administrator
3. Make sure the game is listed in OpenKneeboard's settings and the path is correct
4. Either make sure both the pen and button settings are set for 'all applications', or set them both for OpenKneeboard and the game
5. Uninstall **all** drivers from Huion and others, e.g. VRKneeboard's driver, OpenTabletDriver, or the various specialized drivers for the game 'osu!'; this includes the drivers you're currently using
6. Install the latest Huion drivers
7. Install the latest [Microsoft Visual C++ Redistributables](https://aka.ms/vs/17/release/vc_redist.x64.exe)

It is very important that you uninstall then reinstall the drivers; reinstalling/updating/repairing is not enough.

## Erasing with pen button doesn't work

This section only applies when using Huion's driver.

1. Remove any existing drivers (Huion, VRKneeboard drivers, OpenTabletDriver, any special drivers for 'osu!'
2. Install the latest driver from Huion

## "Press Keys" don't work

1. There is a small switch on one edge of the tablet; try switching it to the other position. This should be labelled as "Press Key Lock" in the manual for your specific tablet.
2. Remove any existing drivers
3. Install the latest driver from Huion

## Only a small area of the tablet works

* Set rotation to 0 in the Huion app; use OpenKneeboard's setting instead
* In the Huion app, set the work area to "All Displays" (especially for WMR headsets)
