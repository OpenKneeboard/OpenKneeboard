---
has_children: true
nav_order: 2
---

# Compatibility

OpenKneeboard requires:

- x64 Windows Home or Pro that is still under 'General Availability' support from Microsoft; as of 2024-07-08, this means Windows 10 22H2 or newer, or Windows 11 22H2 or newer. You can see your current version by opening the start menu, typing 'winver', and running the winver program when it appears
  - For up-to-date information on supported versions of Windows, see Microsoft's lifecycle documentation for [Windows 10](https://learn.microsoft.com/en-us/lifecycle/products/windows-10-home-and-pro) or [Windows 11](https://learn.microsoft.com/en-us/lifecycle/products/windows-11-home-and-pro).
  - If you are running Windows 10 N, Windows 10 KN, Windows 11 N, or Windows 11 KN, you may need to [install the Media Feature Pack](https://support.microsoft.com/en-us/topic/media-feature-pack-list-for-windows-n-editions-c1c6fffa-d052-8338-7a79-a4bb980a700a). These versions of Windows are available in Europe and South Korea in order to comply with local regulations, and by default omit Windows components that are required by OpenKneeboard for image and other media handling.
  - Enterprise, server, LTSC, and other non-consumer editions of Windows are not supported, as they may be missing required Windows components.
- a graphics card and driver that supports Direct3D 11.1

OpenKneeboard [will not run as Administrator, or with User Account Control (UAC) turned off](https://openkneeboard.com/troubleshooting/elevation/).

