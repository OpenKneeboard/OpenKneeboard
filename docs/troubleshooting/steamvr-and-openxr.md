# Troubleshooting SteamVR and OpenXR

## Game Crashes

In OpenKneeboard's Games List, change 'Rendering API' from 'Auto-detect' to SteamVR or OpenXR; this isn't usually needed, but reduces the changes of incompatibilities with other programs.

If this does not solve the problem, try unplugging any graphics tablets (e.g. Wacom or Huion) and restarting OpenKneeboard. If that fixes the problem, please file a GitHub Issue.

## Kneeboard Is Not Visible

- Reset OpenKneeboard's VR settings to defaults
- Try binding a button to 'Recenter' in OpenKneeboard's Input settings; it usually works best to to use the same keys/buttons as you use for in-game VR recenter
- If the game is running as administrator, you must run OpenKneeboard as administrator.
