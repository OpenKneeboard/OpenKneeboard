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

- UWP apps are not supported at all. These generally show up in task manager with a 'Runtime Broker' process
- Apps with specialized mouse handling may have problems, or may work better with a tablet than the mouse

This not necessarily a bug in the apps; Windows only supports mouse interaction with the active Window. OpenKneeboard attempts to work around this restriction, but it is not how Windows, apps, or their frameworks are designed to work.

### Specific apps with compatibility issues

- Windows Calculator (UWP)
- Map drag-and-drop in LittleNavMap
- DCSTheWay
