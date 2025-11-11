---
has_children: true
nav_order: 2
---

# Compatibility

OpenKneeboard requires:

- x64 Windows Home or Pro that is still under 'General Availability' support from Microsoft 
  - as of 2025-11-11, this means Windows 11 23H2 or newer
  - as of 2025-11-11, Windows 10 is no longer under 'General Availability' support. However, while no longer tested or officially supported, there are no known issues under Windows 10 22H2 with Extended Security Updates (ESU).
  - You can see your current version by opening the start menu, typing 'winver', and running the winver program when it appears
  - For up-to-date information on supported versions of Windows, see Microsoft's lifecycle documentation for [Windows 11](https://learn.microsoft.com/en-us/lifecycle/products/windows-11-home-and-pro).
  - If you are running a regional variant of Windows such as Windows N or KN, you may need to [install the Media Feature Pack](https://support.microsoft.com/en-us/topic/media-feature-pack-list-for-windows-n-editions-c1c6fffa-d052-8338-7a79-a4bb980a700a). These versions of Windows are available in Europe and South Korea in order to comply with local regulations, and by default omit Windows components that are required by OpenKneeboard for image and other media handling.
  - Enterprise, server, LTSC, and other non-consumer editions of Windows are not supported because they may be missing required Windows components.
- a graphics card and driver that supports Direct3D 11.1
- [a non-Administrator user account, with User Account Control (UAC) enabled](https://openkneeboard.com/troubleshooting/elevation/)
- Third-party repackaged versions of Windows (often labeled as 'debloated', 'optimized', or 'streamlined') are not supported, as they often remove required Windows components, use an Administrator account, or disable/remove UAC.

