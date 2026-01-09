---
parent: Features
redirect_from:
  - /how-to/huion/
  - /how-to/wacom/
  - /how-to/huion.html
  - /how-to/wacom.html
---

# Graphics Tablets

Graphics tablets (eg Wacom) are supported, *but not required*. These are generally marketed towards artists and
designers.

**Phones and tablet computers such as an iPad, Microsoft Surface, or similar are a different kind of device and not
supported**.

Graphics tablets can be used to:

- interact with the in-game toolbar: switch pages, tabs, profiles, toggle bookmarks and so on
- sketch or take notes
- click on links in PDF documents
- navigate the table of contents in PDF documents
- interact with [captured windows](./window-capture.md)

## Which tablet to get

- The Wacom Intuos S is a good size, with buttons that are easy to find in VR
- The Huion H640P is also a popular choice, and often cheaper
- Alternatively, [any tablet supported by OpenTabletDriver](https://opentabletdriver.net/Tablets) can be used

## How to use your tablet

OpenTabletDriver is *strongly* recommended instead of your manufacturer's drivers.

1. Uninstall your manufacturer's driver from 'Add or Remove Programs'; try searching for your manufacturer's name, '
   pen', and 'tablet'. There may be multiple entries for your tablet - remove them all.
2. Use [TabletDriverCleanup](https://github.com/X9VoiD/TabletDriverCleanup) to remove left-over traces
3. Install [OpenTabletDriver](https://opentabletdriver.net/)
4. Install [OTD-IPC](https://go.openkneeboard.com/otd-ipc)

OpenTabletDriver is only active while it is running;
see [OpenTabletDriver's FAQ](https://opentabletdriver.net/Wiki/FAQ/Windows#startup) for advise on starting it
automatically.

**DO NOT** install WinUSB/Zadig unless the OpenTabletDriver website says it is required *for your specific tablet*; if
you install it when it is not needed, it will stop your tablet from working.

## Using your manufacturer's drivers

Use OpenTabletDriver instead if at all possible. If you use your manufacturer's drivers, you should expect to be unable
to bind buttons or erase in OpenKneeboard. This is a limitation of the manufacturer drivers, and can only be fixed by
the manufacturer.

If you must use vendor drivers, [wintab-adapter](https://github.com/OpenKneeboard/wintab-adapter/blob/master/README.md)
is the best way to use vendor drivers with OpenKneeboard, instead of OpenKneeboard's built-in WinTab support.
OpenKneeboard's built-in WinTab support will be removed in a future version.

If your manufacturer driver supports setting the buttons to launch programs, you may find
the [remote controls](remote-controls.md) useful.