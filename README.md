# OpenKneeboard

OpenKneeboard is an open source Kneeboard application, primarily aimed at virtual reality simulators, such as DCS World.

## Current Status

This is best thought of as a 'developer preview': it works for me, on my computer, but has not been significantly used by others. The settings UI and documentation are particularly lacking at present.

## WARNING: Anti-Cheat

While OpenKneeboard is not a cheat, it does hook into the games rendering pipeline, which over-eager anti-cheat systems may consider suspicious. While this is similar to how other overlays work (e.g. Steam and Discord), it is possible that using OpenKneeboard may lead to an anti-cheat ban.

Note that OpenKneeboard has **NO WARRANTY**; see [the LICENSE file](LICENSE) or [full text of the GPLv2](gpl-2.0.txt) for details.

## Getting Help

I make this for my own use, and I share this in the hope others find it useful; I'm not able to commit to support, bug fixes, or feature development.

Support may be available from the community via:

- [GitHub Discussions](https://github.com/fredemmott/OpenKneeboard/discussions)
- [Discord](https://discord.gg/CWrvKfuff3)

I am not able to respond to 1:1 requests for help via any means, including GitHub, Discord, Twitter, Reddit, or email.

## Supported Games

- Non-VR: DirectX 11
- OpenVR: any
- Oculus: DirectX 11, DirectX 12

OpenKneeboard has been primarily tested with DCS: World via the Oculus SDK, using DirectX 11.

## Features

- DirectInput control: previous/next tab/page. Up to 128 buttons are supported ;)
- Folder Tab: shows images from a folder. Recommended 3:4 aspect ratio
- DCS Radio Log Tab: log of all radio messages that are shown by DCS World as text in the top left corner. There is no voice transcription. This is especially useful for single player campaigns.
- DCS Mission Tab: any mission-specific kneeboard pages.
- DCS Aircraft Tab: any aircraft-specific kneeboard pages.
- DCS Terrain Tab: any terrain-specific kneeboard pages. I believe these are currently only available for Caucasus and Nevada

## Developer Information

[![Continuous Integration](https://github.com/fredemmott/OpenKneeboard/actions/workflows/ci.yml/badge.svg)](https://github.com/fredemmott/OpenKneeboard/actions/workflows/ci.yml)

- This project is written in C++20
- It is built with CMake
- It is primarily developed with Visual Studio Code, using the compilers from Visual Studio 2022
- The UI is built with wxWidgets

For details on some internals, see [docs/internals/](docs/internals/).

## License

OpenKneeboard is licensed under the GNU General Public License, version 2.

This project uses several third-party libraries, which are used and distributed under their own license terms.
