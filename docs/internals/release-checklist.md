# Release checklist

## Before Release

* Update `Quick Start.pdf`
* Update the `GameEvent` path if the format of existing messages has changed in a backwards-incompatible way
* Check version number in CMakeLists.txt
* Update sponsors list in about page

## Testing

Test all of these with the `RelWithDebInfo` msi package, as it's the most restricted form of installation.

* SteamVR
* Oculus: DX11 and DX12
* OpenXR: DX11 and DX12
* Non-VR: DX11
* Gaze detection, zoomed and unzoomed (`hello_xr` is useful for testing), before and after centering
* Wacom tablet: drawing, erasing, PDF navigation, expresskeys
* Huion tablet: drawing, erasing, PDF navigation, presskeys + remote control executables
* Variable aspect ratios
* Drawing perf/feel while in VR
* Bindings
  * Physical joystick/throttle
  * XBox controller
  * VJoy controller
  * Mouse buttons
  * Keyboard
  * Multiple buttons (combos)
* adding, removing, reordering tabs, including DCS tabs
* expected default tabs with quick start guide on fresh install
* adding, removing games
* finding DCS from empty games list
* DCS integration test utilities: `fake-dcs.exe` and `test-gameevent-feeder.exe`
* DCS: play full single-player mission in Caucasus or NTTR
  * Briefing tab
    * Text
    * Images
    * A-10C CDU wind
  * Mission kneeboard
  * Radio log
  * Theater kneeboard (only in Caucasus and NTTR)
* DCS: restart mission
* DCS: switch to a different campaign
* DCS: restart DCS. Start multiplayer mission. Check DCS tabs.

## Releasing

* Push tag
* Wait for GitHub Actions to create a draft release with files and template release notes
* Edit release notes
* Publish
* Update DCS User Files, Discord, Reddit etc
