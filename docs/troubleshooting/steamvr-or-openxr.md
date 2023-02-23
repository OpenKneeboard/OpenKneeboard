---
title: SteamVR or OpenXR
parent: Troubleshooting
---

# Troubleshooting SteamVR or OpenXR Issues

## Game Crashes

In OpenKneeboard's Games List, change 'Rendering API' from 'Auto-detect' to SteamVR or OpenXR; this isn't usually needed, but reduces the changes of incompatibilities with other programs.

If this does not solve the problem, try unplugging any graphics tablets (e.g. Wacom or Huion) and restarting OpenKneeboard. If that fixes the problem, please file a GitHub Issue.

## Kneeboard Is Not Visible

- Reset OpenKneeboard's VR settings to defaults.
- Try binding a button to 'Recenter' in OpenKneeboard's Input settings; it usually works best to to use the same keys/buttons as you use for in-game VR recenter.
- Do not run the game elevated/as administrator.
- Follow the [additional steps](wmr-kneeboard-position.md) for WMR if you are using a WMR headset (e.g. HP Reverb G2).

## Kneeboard Is Visible In SteamVR, But Not OpenXR

Select "Enable for all users" for OpenXR, in OpenKneeboard's VR settings page.
