---
parent: Troubleshooting
---

# Known Issues

## OpenKneeboard freezes on some corrupted DCS `.miz` or .`trk` files

OpenKneeboard v1.12.6 and below do not correctly handle some forms of zip file corruption, which can cause an OpenKneeboard freeze, but not a game freeze. This will be corrected later versions.

## Meta Link (wired) disconnects when using OpenTabletDriver

Meta introduced a firmware change in late 2025 which triggers this issue; the next version of OpenTabletDriver (v0.6.7) is expected to include a workaround.

For now, your options are:
- use Air Link, Virtual Desktop, Steam Link, or another alternative.
- use your vendor-provided drivers instead; [wintab-adapter](https://github.com/OpenKneeboard/wintab-adapter) is recommended.
- use a development build of OpenTabletDriver.

## Crash or error on NVidia 591+ drivers with USB or virtual monitors

See [the dedicated documentation for this issue](d3d11-unavailable.md).

## Severe performance issues with Pimax headsets on AMD graphics cards

This affects all OpenXR overlays (e.g. OpenKneeboard, RaceLabs VR, OpenXR Toolkit's menus).

This is a Pimax issue, and can only be fixed by Pimax; contact Pimax for support.

## Problems when using XRNeckSafer

XRNeckSafer [has](https://gitlab.com/NobiWan/xrnecksafer/-/issues/25) [several](https://gitlab.com/NobiWan/xrnecksafer/-/issues/16) [bugs](https://gitlab.com/NobiWan/xrnecksafer/-/issues/15) that can cause crashes or other issues in software that uses OpenXR correctly, but not in the specific ways that XRNeckSafer expects.

These are not OpenKneeboard issues; you can:
- use VRNeckSafer with SteamVR instead
- check for workarounds in [the XRNeckSafer discord](https://discord.gg/pwcxxTE8TF)
- see [the more general FAQ](../faq/index.md#i-use-a-tool-that-changes-how-my-real-world-movement-affects-in-game-movement-how-do-i-use-it-with-openkneeboard) for these kinds of issues

## Degraded performance when system capacity is exceeded

OpenKneeboard is extremely efficient, but it does need *some* GPU and CPU resources; if you are exceeding your systems capabilities already, adding any additional software will make it disproportionately worse.

In practice, this mostly seems to be an issue for people playing iRacing; common causes include:

- reaching GPU thermal limits, even without OpenKneeboard
- reaching GPU power draw limits, even without OpenKneeboard
- exceeding VRAM capacity, even without OpenKneeboard - especially if adding additional liveries/paint jobs in iRacing, e.g. via Trading Paints

You need to lower your settings to avoid exceeding your systems' capabilities.

## ~~Various positinoing issues with Varjo Base~~

All known issues in Varjo Base affecting OpenKneeboard were fixed in v4.14 (December 2025).