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

### Pimax headsets with AMD graphics cards

On some systems with AMD graphics card, the Pimax software has significant performance issues rendering *any* OpenXR overlay; this can only be fixed by Pimax.

### Varjo headsets

Varjo Base v4.4 and above [have a bug](https://github.com/OpenKneeboard/OpenKneeboard/issues/698) which makes OpenKneeboard invisible or appear in an extremely incorrect position for some users. If you encounter any issues, downgrade Varjo Base to v4.3 or below. Varjo are aware of the issue.

## XBox and other XInput Controllers

These are only supported when OpenKneeboard is the active window; this is a Windows "feature" specific to XBox controllers.

## Other Controllers

Any other DirectInput game controller should be supported; this includes the vast majority of button boxes, joysticks, throttles, etc.

## Graphics Tablets

Graphics tablets are supported, *but not required*; OpenKneeboard supports most graphics tablets; these are generally marketed to artists. **Phones and tablet computers such as an iPad, Microsoft Surface, or similar are a different kind of device and not supported.**

- Wacom tablets seem to have the most trouble-free drivers; the small Intuos S is the most popular
- Huion tablets are also popular and well-tested, especially the H640P; the drivers do have noteable issues leading to more complicated setup instructions

[OpenTabletDriver](https://go.openkneeboard.com/otd-ipc) is *STRONGLY* recommended instead of your tablet manufacturer's drivers.