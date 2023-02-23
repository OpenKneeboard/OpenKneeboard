---
title: Oculus or Non-VR
parent: Troubleshooting
---

# Troubleshooting Oculus or Non-VR Issues

## Kneeboard Is Not Visible

Restart DCS after each step to see if it's fixed.

1. Do not run the game or any launchers (e.g. SkateZilla) as administrator/elevated.
2. Make sure that the game is in OpenKneeboard's games list, and the path is correct.
3. Install the latest [Microsoft Visual C++ Redistributables](https://aka.ms/vs/17/release/vc_redist.x64.exe)
4. If using RivaTuner Statistics Server or the MSI Afterburner overlay, turn on 'Use Microsoft Detours API hooking' at the bottom of the overlay profile's setup page in RTSS/Afterburner
5. Also check everything in the 'Game Crashes' section below

## Game Crashes

Restart DCS after each step to see if it's fixed.

1. Try setting the Rendering API explicitly instead of 'Auto-detect' in OpenKneeboard's game settings
2. Remove any `dxgi.dll` or `d3d11.dll` from the same directory as the game executable, e.g. DCS's `bin/` directory. **Do not remove these from any other location**. These are often used for shader or performance modifications.
3. If that doesn't work, disable/close any other programs that can draw in the game window. This includes any other in-game overlays, both VR and non-VR, for example in Discord, Steam (the normal in-game overlay, not SteamVR), Oculus Tray Tool, Virtual Kneeboard, NVidia GeForce Experience, RivaTuna Statistics Server, MSI Afterburner

Once you have a working setup, re-enable them and try again. Once you've identified what causes the problem, please file a GitHub issue.
