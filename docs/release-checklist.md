# Release checklist

## Testing

Test all of these with the `RelWithDebInfo` msix package, as it's the most restricted form of installation.

* SteamVR
  * Gaze zoom before and after recentering
* Oculus: DX11 and DX12
* OpenXR: DX11
* Non-VR: DX11
* Wacom tablet: drawing, erasing, PDF navigation, expresskeys
* Huion tablet: drawing, erasing, PDF navigation, presskeys + remote control executables
* Variable aspect ratios
* Drawing perf/feel while in VR
* XBox controller
* Physical joystick/throttle
* VJoy controller
* adding, removing, reordering tabs
* adding, removing games
* finding DCS from empty games list
* DCS integration test utilities: `fake-dcs.exe` and `test-gameevent-feeder.exe`
* DCS: play full single-player mission in Caucasus or NTTR
  * Mission kneeboard
  * Radio log
  * Theater kneeboard (only in Caucasus and NTTR)
* DCS: restart mission
* DCS: switch to a different campaign

## Releasing

* Check version number in CMakeLists.txt is up to date
* Test msix from GitHub Actions as above
* Push tag
* Wait for GitHub Actions to create a draft release with files and template release notes
* Edit release notes
* Publish
* Update DCS User Files, Discord, Reddit etc
