---
parent: Troubleshooting
---

# Known Issues

## Incorrect time/timezone is shown

OpenKneeboard will show UTC instead of the correct local time if the DLLs included with Windows do not recognize the current timezone.

In October 2026, Microsoft added new timezones in:

- Alberta
- British Columbia
- Manitoba

However, as of 2026-10-16, Microsoft has not added full support for these timezones to the DLLs included in Windows, including `icu.dll`, which is used by Microsoft's C++ runtime to provide timezone support.

## iRacing

Set `DepthLayerExtensionEnabled` to `0` in your iRacing OpenXR ini file.

iRacing is one of very few games that submits depth; this has uncovered bugs in various OpenXR runtimes, including:

- overlays obscured by cockpit (Meta Link)
- IPD issues (Vive Focus 2 on SteamVR)
- performance (various)
- 'watery' artifacts (various)

If you are affected by an issue which is resolved by `DepthLayerExtensionEnabled=0`, contact your runtime vendor (usually your headset manufacturer) for support.

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

OpenKneeboard is typically extremely efficient, but it does need *some* GPU and CPU resources; if you are exceeding your systems capabilities already, adding any additional software will make it disproportionately worse.

In practice, this mostly seems to be an issue for people playing iRacing; common causes include:

- reaching GPU thermal limits, even without OpenKneeboard
- reaching GPU power draw limits, even without OpenKneeboard
- exceeding VRAM capacity, even without OpenKneeboard - especially if adding additional liveries/paint jobs in iRacing, e.g. via Trading Paints

You need to lower your settings to avoid exceeding your systems' capabilities.

## Performance with TinyPedal

Using TinyPedal with OpenKneeboard typically involves capturing many small windows, leading to very high overhead:

- Window capture is not an efficient way to add content to VR, especially as the number of captured windows increases.
- Many headsets and runtimes have performance issues with composition layers (overlays) which get worse as the number of layers increases. Alternative approaches introduce significant jitter which would particularly harm readability for small overlays like TinyPedal, and could also lead to anti-cheat false positives.
- OpenKneeboard makes some 'GPU time vs VRAM usage' tradeoffs which are optimized for large, detailed overlays (like kneeboards), but extremely suboptimal for lots of small windows.

If this affects you, you can:

- reduce the number of windows you are capturing
- reduce the number of VR views you have enabled
- use alternative tools that typically combine more information into single overlays

## ~~Various positioning issues with Varjo Base~~

All known issues in Varjo Base affecting OpenKneeboard were fixed in v4.14 (December 2025).

## ~~Meta Link (wired) disconnects when using OpenTabletDriver~~

Up-to-date versions of OpenTabletDriver work around this Meta firmware issue.

