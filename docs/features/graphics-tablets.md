---
parent: Features
---

# Graphics Tablets

Graphics tablets are supported, *but not required*; for information on supported devices, see [the compatibility notes](../compatibility/hardware.md#graphics-tablets).

Graphics tablets can be used to:

- interact with the in-game toolbar: switch pages, tabs, profiles, toggle bookmarks and so on
- sketch or take notes
- click on links in PDF documents
- navigate the table of contents in PDF documents
- interact with [captured windows](./window-capture.md)

Huion and Wacom are the most commonly used tablet manufacturers for OpenKneeboard users; for more information on these, see:

- **Huion**: [how-to](../how-to/huion.md) - [troubleshooting](../troubleshooting/huion-tablet.md)
- **Wacom**: [how-to](../how-to/wacom.md) - [troubleshooting](../troubleshooting/wacom-tablet.md)

## Other Manufacturers

OpenKneeboard *should* work with any WinTab or OpenTabletDriver-compatible tablet, however, as of 2024-04-21, most manufacturers (including Wacom) have limitations or bugs in their WinTab drivers which prevent their use with OpenKneeboard.

You have a choice between "Standard" WinTab, "Invasive" WinTab, and "OpenTabletDriver"
- OpenTabletDriver is *strongly* recommended
- WinTab of both forms is legacy-only; no support is available, and issues won't be investigated
- "Invasive" WinTab mode may be needed for some buggy drivers, and *will* be removed in a future version of OpenKneeboard
- "Standard" WinTab mode is *likely* to be removed in a future version of OpenKneeboard, due to driver bugs frequently causing crashes

If you are currently using "Invasive" WinTab:
- try OpenTabletDriver instead
- if WinTab Standard does not work, ask your tablet manufacturer to support:
  - sending `WT_CTXOVERLAP` messages to non-foreground windows
  - calls to `WTOverlap(ctx, TRUE)` from non-foreground windows