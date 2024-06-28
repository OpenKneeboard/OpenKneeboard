---
title: Non-VR or Legacy Oculus API (LibOVR)
parent: Troubleshooting
---

# Troubleshooting Non-VR or Legacy Oculus API (LibOVR) Issues

**Don't waste time on non-VR issues if you will only actually play the game with SteamVR or OpenXR**: fixing non-VR issues makes it slightly
*less* likely that SteamVR or OpenXR will work.

## Kneeboard Is Not Visible

Restart the game after each step to see if it's fixed.

1. Do not run the game or any launchers (e.g. SkateZilla) as administrator/elevated.
2. Do not run OpenKneeboard as administrator/elevated.
3. Make sure that the game is in OpenKneeboard's games list, and the path is correct.
4. If using RivaTuner Statistics Server or the MSI Afterburner overlay, turn on 'Use Microsoft Detours API hooking' at the bottom of the overlay profile's setup page in RTSS/Afterburner
5. **Also try everything in the 'Game Crashes' section below**, even if the game is not crashing

## Game Crashes

Restart the game after each step to see if it's fixed.

1. Try setting the Rendering API explicitly instead of 'Auto-detect' in OpenKneeboard's game settings; as of 2024-05-21, you most likely want "Direct3D 11 (Non-VR)"
2. Remove any `dxgi.dll` or `d3d11.dll` from the same folder as the game executable, e.g. DCS's `bin` or `bin-mt` folders. **Do not remove these from any other location**. These are often used for shader or performance modifications.
3. If that doesn't work, disable/close any other programs that can draw in the game window. This includes any other in-game overlays, both VR and non-VR, for example in Discord, Steam (the normal in-game overlay, not SteamVR), Oculus Tray Tool, Virtual Kneeboard, NVidia GeForce Experience, RivaTuna Statistics Server, MSI Afterburner

Once you have a working setup, re-enable them and try again. Once you've identified what causes the problem, please file a GitHub issue.
