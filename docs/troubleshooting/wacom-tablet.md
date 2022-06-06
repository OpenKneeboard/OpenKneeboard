# Troubleshooting Wacom Tablets


## Tablet only works properly when OpenKneeboard is the active window

### Symptoms

- Tablet acts a mouse in-game
- Pen buttons don't work in-game
- "Press keys" don't work in-game

### Fixes

1. Make the sure the game (and any launchers, e.g. Skatezilla) are not running elevated/as administrator
2. Make sure the game is listed in OpenKneeboard's settings and the path is correct
3. Either make sure both the pen and button settings are set for 'all applications', or set them both for OpenKneeboard and the game
4. Reinstall the latest drivers from Wacom

### Root cause

Wintab tablets always only work with the currently active window. OpenKneeboard extends the games to support the tablet, and pass your pen strokes and button presses back to OpenKneeboard.

As Windows will not allow a non-elevated process to modify an elevated process, the game can not run elevated.

OpenKneeboard will only modify processes in its' games list to reduce the chances of side-effects on other processes.
