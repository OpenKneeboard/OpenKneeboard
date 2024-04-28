---
title: Wacom Tablets
parent: Troubleshooting
---

# Troubleshooting Wacom Tablets

If you're having problems, you might want to [try OpenTabletDriver](https://go.openkneeboard.com/otd-ipc); the rest of this document only applies to Wacom's driver.

## Unable to bind any buttons

If your Wacom driver is v6.4.6 or later, uninstall the Wacom driver, and install driver v6.4.5 or earlier, as Wacom have removed necessary features from the v6.4.6 driver.

If your tablet or system requires a newer driver you can:

- ask Wacom to re-add support for "Application Defined" ExpressKey and pen button behaviors
- use OpenTabletDriver instead

## Tablet only works properly when OpenKneeboard is the active window

### Symptoms

- Tablet acts a mouse in-game
- Pen buttons don't work in-game
- "Press keys" don't work in-game

### Fixes

1. Make sure that wintab is set to 'standard' in OpenKneeboard's input settings
2. Make the sure the game (and any launchers, e.g. Skatezilla) are not running elevated/as administrator
3. Make sure the game is listed in OpenKneeboard's settings and the path is correct
4. Either make sure both the pen and button settings are set for 'all applications', or set them both for OpenKneeboard and the game
5. Reinstall the latest drivers from Wacom
