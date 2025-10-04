---
parent: Compatibility
title: Hardware
---

# Hardware Compatibility

## Graphics Cards

OpenKneeboard requires a graphics card that is compatible with Direct3D 11.1, even if the game is not using Direct3D.

## VR Headsets

OpenKneeboard is primarily developed with a Quest Pro, and the majority of users use an HP Reverb G2.

It has also been heavily tested with the Quest 2, and is known to work with Pimax and Varjo headsets.

It should work with any headset that is compatible with OpenXR or SteamVR.

### Known issues

- Pimax + AMD: on some systems, the Pimax software has significant performance issues rendering *any* OpenXR overlay; this can only be fixed by Pimax.
- The Somnium VR1 OpenXR runtime appears to have bugs in the handling of overlays which can cause game crashes or incorrect overlay positions.
  - There are unconfirmed reports that these issues may have been fixed in their 'beta4' runtime (June 2025)
- Varjo Base v4.4 and above [have a bug](https://github.com/OpenKneeboard/OpenKneeboard/issues/698) which makes OpenKneeboard invisible or appear in an extremely incorrect position for some users. If you encounter any issues, downgrade Varjo Base to v4.3 or below. Varjo are aware of the issue.
  - There are unconfirmed reports that these issues may have been fixed in Varjo Base v4.11
  - If you have used OpenKneeboard with Varjo Base v4.4-v4.10, you *may* need to reset and reconfigure
    position after switching to Varjo Base v4.3 or v4.11+

## XBox and other XInput Controllers

These are only supported when OpenKneeboard is the active window; this is a Windows "feature" specific to XBox controllers.

## Other Controllers

Any other DirectInput game controller should be supported; this includes the vast majority of button boxes, joysticks, throttles, etc.

## Graphics Tablets

Graphics tablets are supported, *but not required*; OpenKneeboard supports most graphics tablets; these are generally marketed to artists. **Phones and tablet computers such as an iPad, Microsoft Surface, or similar are a different kind of device and not supported.**

[OpenTabletDriver](https://go.openkneeboard.com/otd-ipc) is *STRONGLY* recommended instead of your tablet manufacturer's drivers; as of 2025-10-04, I am not aware of any manufacturers with reliable and full-featured drivers; use OpenTabletDriver instead.

- The Wacom Intuos S is a good size, with buttons that are easy to find in VR
- The Huion H640P is also a popular choice, and often cheaper
- Alternatively, [any tablet supported by OpenTabletDriver](https://opentabletdriver.net/Tablets) can be used