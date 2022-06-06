# Huion Tablets

## Tablet only works properly when OpenKneeboard is the active window

### Symptoms

- Tablet acts a mouse in-game
- Pen buttons don't work in-game
- "Press keys" don't work in-game

### Fixes

1. Make the sure the game (and any launchers, e.g. Skatezilla) are not running elevated/as administrator
2. Make sure the game is listed in OpenKneeboard's settings and the path is correct
3. Either make sure both the pen and button settings are set for 'all applications', or set them both for OpenKneeboard and the game
4. Uninstall **all** drivers from Huion or others, e.g. VRKneeboard's driver, OpenTabletDriver, or the various specialized drivers for the game 'osu!'
5. Install the latest Huion drivers

### Root cause

Wintab tablets always only work with the currently active window. OpenKneeboard extends the games to support the tablet, and pass your pen strokes and button presses back to OpenKneeboard.

As Windows will not allow a non-elevated process to modify an elevated process, the game can not run elevated.

OpenKneeboard will only modify processes in its' games list to reduce the chances of side-effects on other processes.

## Erasing with pen button doesn't work

This feature requires recent Huion drivers.

1. Remove any existing drivers (Huion, VRKneeboard drivers, OpenTabletDriver, any special drivers for 'osu!'
2. Install the latest driver from Huion

## "Press Keys" don't work

1. There is a small switch on one edge of the tablet; try switching it to the other position. This should be labelled as "Press Key Lock" in the manual for your specific tablet.
2. Remove any existing drivers
3. Install the latest driver from Huion

## Only a small area of the tablet works

* Set rotation to 0 in the Huion app; use OpenKneeboard's setting instead
* In the Huion app, set the work area to "All Displays" (especially for WMR headsets)
