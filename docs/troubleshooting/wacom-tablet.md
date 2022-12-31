# Troubleshooting Wacom Tablets

If you're on v1.3+ and having problems, you might want to [try OpenTabletDriver](https://go.openkneeboard.com/otd-ipc); the rest of this document only applies to Wacom's driver.

## Tablet only works properly when OpenKneeboard is the active window

### Symptoms

- Tablet acts a mouse in-game
- Pen buttons don't work in-game
- "Press keys" don't work in-game

### Fixes

1. (v1.3+): make sure that wintab is set to 'standard' in OpenKneeboard's input settings
2. Make the sure the game (and any launchers, e.g. Skatezilla) are not running elevated/as administrator
3. Make sure the game is listed in OpenKneeboard's settings and the path is correct
4. Either make sure both the pen and button settings are set for 'all applications', or set them both for OpenKneeboard and the game
5. Reinstall the latest drivers from Wacom
6. Install the latest [Microsoft Visual C++ Redistributables](https://aka.ms/vs/17/release/vc_redist.x64.exe)
