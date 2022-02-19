# Release checklist

## Testing

Test all of these with the `RelWithDebInfo` msix package, as it's the most restricted form of installation.

* SteamVR
* Oculus: DX11 and DX12
* Non-VR: DX11
* Wacom tablet: drawing, erasing, PDF navigation
* Huion tablet: drawing, erasing, PDF navigation
* XBox controller
* Physical joystick/throttle
* VJoy controller
* DCS integration test utilities: `fake-dcs.exe` and `test-gameevent-feeder.exe`
* DCS: play full single-player mission in Caucasus or NTTR
  * Mission kneeboard
  * Radio log
  * Theater kneeboard (only in Caucasus and NTTR)

## Releasing

* Check version number in CMakeLists.txt is up to date
* Test msix from GitHub Actions as above
* Push tag
* Wait for GitHub Actions to create a draft release with files and template release notes
* Edit release notes
* Publish
* Update DCS User Files, Discord, Reddit etc
