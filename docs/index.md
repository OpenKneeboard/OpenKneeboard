---
layout: page
title: OpenKneebaord
tagline: Interactive information in your simulators
---

OpenKneeboard is a way to show reference information and take notes in games - especially flight simulators - including in VR. 

OpenKneeboard can be controlled via joystick/HOTAS bindings, or a graphics tablet ('artists tablet') such as those made by Wacom or Huion; phones and tablet computers like iPads or Microsoft Surface tablets are **not** compatible.

## Getting Started

Note that OpenKneeboard has **NO WARRANTY**.

1. Download [the latest release](https://github.com/OpenKneeboard/OpenKneeboard/releases/latest)
2. Install it; the installer will offer to launch OpenKneeboard when finished.
3. Open settings
4. Add games to the 'Games' tab if they were not automatically added; this is needed for tablet, Oculus, and non-VR support
5. Configure your bindings; if you're playing in VR, you probably want to bind 'recenter' to the same buttons you use for the game.

You might also be interested in the documentation for:

* [Huion tablet users](huion.md)
* [StreamDeck users](streamdeck.md)
* [Wacom tablet users(wacom.md)

## Getting Help

First, [check the troubleshooting guides](troubleshooting/).

I make this for my own use, and I share this in the hope others find it useful; I'm not able to commit to support, bug fixes, or feature development.

Support may be available from the community via:

- [Discord](https://go.openkneeboard.com/discord)
- [GitHub Discussions](https://github.com/OpenKneeboard/OpenKneeboard/discussions)

I am not able to respond to 1:1 requests for help via any means, including GitHub, Discord, Twitter, Reddit, or email.

## Compatibility

- 64-bit Windows 10 May 2020 Update (version 2004 or 10.0.19041) or newer
- A graphics card that supports Direct3D 11

Windows 11 is untested, and I am unable to investigate any issues reported with OpenKneeboard on Windows 11.

### Notes and in-game toolbar

Taking notes and using the in-game toolbar requires a WinTab-compatible graphics tablet; common choices that are reported to work well include:

- Wacom Intuos S: fewest software issues, large recessed buttons are easy to find with the headset on
- Huion H640P: cheaper, more buttons

The following are **not compatible** with OpenKneeboard:

- Any XP-Pen tablets with the XP-Pen drivers; *might* work [with OpenTabletDriver]
- Any Gaomon tablets with the Gaomon drivers; *might* work [with OpenTabletDriver]
- Any tablet computers, regardless of manufacturer; this includes Apple iPads, Microsoft Surface, etc.
- Phones

### Games

- Non-VR: DirectX 11
- Oculus: DirectX 11, DirectX 12
- OpenVR: any
- OpenXR: DirectX 11, DirectX 12 (except Varjo)

OpenKneeboard has been primarily tested with DCS: World Open Beta via the Oculus SDK, using DirectX 11. Users report a good experience with:

* DCS with an HP Reverb G2 with SteamVR
* DCS with an HP Reverb G2 with OpenComposite
* DCS without VR
* MSFS with DX11 with various headsets
* MSFS with DX12 with various headsets (except Varjo)

Varjo support for games using DirectX 12 in combination with OpenXR is expected in OpenKneeboard v1.2.

**WARNING: Anti-Cheat**

While OpenKneeboard is not a cheat, it does hook into the games rendering and input pipelines, which over-eager anti-cheat systems may consider ban-worthy. While this is similar to how other overlays work (e.g. Steam and Discord), it is possible that using OpenKneeboard may lead to an anti-cheat ban.

Note that OpenKneeboard has **NO WARRANTY**; see [the LICENSE file](LICENSE) and [full text of the GPLv2](gpl-2.0.txt) for details.

## Thanks

- Paul 'Goldwolf" Whittingham for the logo and banner artwork.
- BeamRider for the [VRK](https://forums.eagle.ru/topic/211308-vrk-a-virtual-reality-enabled-kneeboard-with-touch-and-ink-support) project, for showing how useful an in-VR kneeboard is, and clear inspiration for OpenKneeboard.
- [Benjamin Höglinger-Stelzer a.k.a. Nefarius](https://nefarius.at/) for the [Injector](https://github.com/nefarius/Injector) utility, which has been extremely useful for development.
- Nefarius and the other members of the ViGEm Discord for lots of advice/feedback/information on DirectX, Detours, and other aspects of Windows development.
- [AMD GPUOpen's OCAT project](https://gpuopen.com/ocat/) for demonstrating overlay rendering in applications using the Oculus SDK.

## License

OpenKneeboard is licensed under the GNU General Public License, version 2.

This project uses several third-party libraries, which are used and distributed under their own license terms.

## Screenshots

![Screenshot of the main app](screenshots/config-app.png)
![Non-VR Screenshot](screenshots/non-vr.png)
![VR Screenshot](screenshots/theater.png)

[with OpenTabletDriver]: https://go.openkneeboard.com/otd-ipc
