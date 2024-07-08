---
parent: Troubleshooting
---

# Window Capture

Capture will work with most apps, but has limitations.

## Capture

On apps using older frameworks, dialog boxes and drop-down menus may not be shown.

## Cursor

While OpenKneeboard's own cursor will move, it will not move the Windows mouse cursor, as Windows only supports mouse interaction with the active window.

## Mouse/tablet forwarding

If mouse/tablet forwarding does not work for you with a specific app, it is simply incompatible. There is no configuration, no troubleshooting, or anything else you can do to fix it, [unless you are a C++ developer familiar with Win32 and your framework](../faq/third-party-developers.md#why-doesnt-the-mouse-emulation-work-correctly-in-my-app-when-using-window-capture-tabs).

Apps, frameworks, and Windows itself try very hard to make it so mouse movement only affects active window; sometimes, despite OpenKneeboards' best efforts, they succeed.

These issues are very time consuming, and there are too many apps/frameworks for me to investigate them. There is no need to ask, or to open bug reports/feature requests.

### Specific apps with compatibility issues

- Windows Calculator (UWP)
- Map drag-and-drop in LittleNavMap
- DCSTheWay
