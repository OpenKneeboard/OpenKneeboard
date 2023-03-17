---
title: SteamVR or OpenXR
parent: Troubleshooting
---

# Troubleshooting SteamVR or OpenXR Issues

## Game Crashes

If OpenKneeboard's Games List, change 'Rendering API' from 'Auto-detect' to SteamVR or OpenXR; this isn't usually needed, but reduces the changes of incompatibilities with other programs. If the game is not in OpenKneeboard's game's list, ignore this - it isn't needed for SteamVR or OpenXR.

If this does not solve the problem, try unplugging any graphics tablets (e.g. Wacom or Huion) and restarting OpenKneeboard. If that fixes the problem, please file a GitHub Issue.

## Kneeboard Is Not Visible

- Reset OpenKneeboard's VR settings to defaults.
- Try binding a button to 'Recenter' in OpenKneeboard's Input settings; it usually works best to to use the same keys/buttons as you use for in-game VR recenter.
- Do not run the game elevated/as administrator.
- Follow the [additional steps](wmr-kneeboard-position.md) for WMR if you are using a WMR headset (e.g. HP Reverb G2).
- If the game is in OpenKneeboard's games list, make sure the API is either set to 'Auto-detect', or 'SteamVR' or 'OpenXR' depending on what you want. If the game is not listed (or if the path is different) that's fine for SteamVR or OpenXR - in which case, ignore this step.

## Kneeboard Is Visible In SteamVR, But Not OpenXR

Make sure OpenXR support is enabled in OpenKneeboard's VR settings page.

## Kneeboard Is Visible In OpenXR, But Not SteamVR

Make sure SteamVR support is enabled in OpenKneeboard's VR settings page.
