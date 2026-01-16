---
parent: Troubleshooting
redirect_from:
  - /troubleshooting/huion-tablet/
  - /troubleshooting/wacom-tablet/
  - /troubleshooting/huion-tablet.html
  - /troubleshooting/wacom-tablet.html
---

# Graphics Tablets

## General advice

- use OpenTabletDriver unless it is completely impossible for you
- if you absolutely must use manufacturer's drivers, use [wintab-adapter] instead of OpenKneeboard's built-in wintab
  support (under Settings -> Input)
- check if the steps in the [how-to](../how-to/graphics-tablets.md) have changed, and go through them again

## The buttons on my pen/tablet don't work

Use OpenTabletDriver.

If you are using OpenTabletDriver, some Huion tablets have a physical switch on the side which disables the buttons on
the tablet. Check the manual for your tablet, and make sure the switch is in the position that enables the buttons.

If you *cannot* use OpenTabletDriver:

- if you have a Wacom tablet supported by v6.4.5 of their driver:
    1. uninstall your current Wacom driver
    2. install v6.4.5
    3. use [wintab-adapter]
- otherwise, ask your manufacturer to add support for application-defined button usage to their WinTab driver

Wacom intentionally removed this feature from later versions of their driver.

If your manufacturer's driver supports launching programs when buttons are pressed, you might find the [remote controls]
useful

## Tablet only works properly when OpenKneeboard is the active window

- Use OpenTabletDriver
- If you *cannot* use OpenTabletDriver, use [wintab-adapter] instead of OpenKneeboard's built-in wintab support
- If that is not enough, ask your tablet manufacturer to fix `WTOverlap()` and `WT_CTXOVERLAP` in their driver

## The tablet doesn't work when the game is the active window

- do not run the game as Administrator
- make sure User Account Control (UAC) is *not* set to the lowest setting. If it is, reboot.
- make sure that OpenTabletDriver (recommended) or wintab-adapter (last resort) is running

## Only a small area of the tablet works

Use OpenTabletDriver. If you *cannot* use OpenTabletDriver, follow the [wintab-adapter] instructions for configuring your tablet.

[wintab-adapter]: https://github.com/OpenKneeboard/wintab-adapter
[remote controls]: ../features/remote-controls.md
