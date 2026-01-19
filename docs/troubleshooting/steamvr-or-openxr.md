---
title: OpenXR or Legacy SteamVR API (OpenVR)
parent: Troubleshooting
---

# Troubleshooting OpenXR or Legacy SteamVR API (OpenVR) Issues

## If you have a Varjo headset and are using Varjo Base v4.4 or above

Varjo Base v4.4 and above [have a bug](https://github.com/OpenKneeboard/OpenKneeboard/issues/698) which makes OpenKneeboard invisible or appear in an extremely incorrect position for some users. Downgrade Varjo Base to v4.3 or below. Varjo are aware of the issue.


## If you using the legacy SteamVR API but the game supports OpenXR

If the game supports OpenXR without using OpenComposite, use OpenXR instead. OpenComposite is unsupported.

## Game crashes

If OpenKneeboard's Games List, change 'Rendering API' from 'Auto-detect' to SteamVR or OpenXR; this isn't usually needed, but reduces the changes of incompatibilities with other programs. If the game is not in OpenKneeboard's game's list, ignore this - it isn't needed for SteamVR or OpenXR.

If this does not solve the problem, try unplugging any graphics tablets (e.g. Wacom or Huion) and restarting OpenKneeboard. If that fixes the problem, please file a GitHub Issue.

## Kneeboard is not visible

- Reset OpenKneeboard's VR settings to defaults.
- Try binding a button to 'Recenter' in OpenKneeboard's Input settings; it usually works best to to use the same keys/buttons as you use for in-game VR recenter, then recenter once you're in-game.
- Do not run OpenKneeboard or the game elevated/as administrator.
- Follow the [additional steps](wmr-kneeboard-position.md) for WMR if you are using a WMR headset (e.g. HP Reverb G2).
- If the game is in OpenKneeboard's games list, make sure the API is either set to 'Auto-detect', or 'SteamVR' or 'OpenXR' depending on what you want. If the game is not listed (or if the path is different) that's fine for SteamVR or OpenXR - in which case, ignore this step.
- If you have a recent version of the Ultraleap driver, use [OpenXR API Layers GUI](https://github.com/fredemmott/OpenXR-API-Layers-GUI) to make the Ultraleap driver the last layer. This is required because current versions of the Ultraleap driver bypass any later layers.

## Kneeboard is visible in SteamVR, but not OpenXR

- Make sure OpenXR support is enabled in OpenKneeboard's VR settings page.
- If you have a recent version of the Ultraleap driver, use [OpenXR API Layers GUI](https://github.com/fredemmott/OpenXR-API-Layers-GUI) to make the Ultraleap driver the last layer. This is required because current versions of the Ultraleap driver bypass any later layers.

## Kneeboard is visible in OpenXR, but not SteamVR

Make sure SteamVR support is enabled in OpenKneeboard's VR settings page.

## Kneeboard flickers or goes backwards in OpenXR

Try turning on the "OpenXR: always update swapchains" option under "Compatibility Quirks" under advanced settings; this option slightly reduces performance.