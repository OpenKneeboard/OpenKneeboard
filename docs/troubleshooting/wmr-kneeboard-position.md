# Kneeboard Position with WMR Headsets

## Problems

- kneeboard is wrong height from the floor
- kneeboard position changes

## Fixes

1. Wait for the WMR home environment to fully load every time, before starting DCS, OpenKneeboard, or SteamVR (if you're using it)
2. Reset the position values back to the defaults
3. [Set your WMR floor height](https://www.windowscentral.com/how-fix-floor-height-windows-mixed-reality)
4. Use SteamVR's recenter feature (if you're using it)
5. Bind a button to recenter the kneeboard in OpenKneeboard's settings; it's usually best to use the same button for in-game recentering
6. If you still have problems, [clear the WMR environment data](https://docs.microsoft.com/en-us/windows/mixed-reality/enthusiast-guide/tracking-system#how-do-i-clear-tracking-and-environment-data) and set it up again

## Root cause

OpenKneeboard always asks to be displayed at the same position, however this must always be relative to some known point - (0, 0, 0). The real-world position of WMR's
(0, 0, 0) can vary; it's not possible for OpenKneeboard to detect or fix this.
