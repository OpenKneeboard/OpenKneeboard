# Oculus and Non-VR Troubleshooting

## Kneeboard Is Not Visible

1. Do not run the game or any launchers (e.g. SkateZilla) as administrator/elevated.
2. Make sure that the game is in OpenKneeboard's games list, and the path is correct.
3. Install the latest [Microsoft Visual C++ Redistributables](https://aka.ms/vs/17/release/vc_redist.x64.exe)

## Game Crashes

1. Try setting the Rendering API explicitly instead of 'Auto-detect' in OpenKneeboard's game settings
2. Remove any `dxgi.dll` or `d3d11.dll` from the same directory as the game executable, e.g. DCS's `bin/` directory. **Do not remove these from any other location**. These are often used for shader or performance modifications.
3. If that doesn't work, disable/close any other programs that can draw in the game window. This includes any other in-game overlays, both VR and non-VR, for example in Discord, Steam (the normal in-game overlay, not SteamVR), Oculus Tray Tool, Virtual Kneeboard, NVidia GeForce Experience, RivaTuna, MSI Afterburner

Once you have a working setup, re-enable them and try again. Once you've identified what causes the problem, please file a GitHub issue.
