---
title: Wacom Tablets
parent: How-To
---

# Using Wacom Tablets with OpenKneeboard

Wacom tablets can either be used with Huion's drivers, or [with OpenTabletDriver](https://go.openkneeboard.com/otd-ipc).

## Using Wacom's drivers

If you're running v1.3, you need to enable the tablet by:

1. Open OpenKneeboard's settings page (bottom left)
2. Go to the input settings page
3. Set WinTab mode to 'standard'

For earlier versions, the tablet is always enabled.

It's easiest to set all of these settings to 'All applications'; otherwise (for example, if you use your
drawing tablet for drawing :p) you'll need to apply these settings both to the OpenKneeboard app, and to
every game you want to use the tablet with. For example, you may need to create a Wacom profile for both
OpenKneeboard and DCS World.

To use the pen for drawing or UIs (e.g. PDF table of contents), you need the
following settings:

![set tip to click; both other buttons 'application defined'](../screenshotswacom-pen-settings.png)

If an on-pen button is being held while drawing, the pen will act as an eraser.

If you also want to use the on-tablet 'Express Keys' to change page/tabs, you'll
also need these settings:

![all expresskeys to 'application defined', disable 'expressview'](../screenshotswacom-tablet-settings.png)

You can then assign the buttons to actions in Edit -> Settings in the app:

![screenshot of settings page with 'wacom tablet' option](../screenshotswacom-expresskey-bindings.png)
