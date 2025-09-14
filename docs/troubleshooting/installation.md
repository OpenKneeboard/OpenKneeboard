---
title: Installation
parent: Troubleshooting
---

# Installation issues

## If the installer finishes, but OpenKneeboard does not start

If OpenKneeboard tells you why it won't start, follow the instructions in the message.

Otherwise:
- make sure you are running [a version of Windows that OpenKneeboard supports](../compatibility/index.md)
- you probably want the documentation for [crashes and freezes](crashes-and-freezes.md)

## If the installer finishes, OpenKneeboard starts, but doesn't work in some other way

This page isn't for you; check for documentation related to your issue in:
- [the FAQ](../faq/index.md)
- [the game compatiblity notes](../compatibility/games.md)
- [the hardware compatiblity notes](../compatibility/hardware.md)
- [the other troubleshooting guides](../troubleshooting/index.md)
- [the feature documentation](../features/index.md)

## If you can't install OpenKneeboard

These steps are *only* for if you are unable to open the installer, or if you get an error *while* installing.

OpenKneeboard uses the Windows Installer service for installation.

- make sure you have downloaded the `msi` ("Microsoft Installer") file - this is the only file you need
- make sure you are running [a version of Windows that OpenKneeboard supports](../compatibility/index.md)
- make sure the "Windows Installer" service is running

If you're having trouble using the MSI on a supported version of Windows with the service running, you need to repair the Windows Installer component of Windows:

- Try [the Windows Installer Troubleshooter](https://support.microsoft.com/en-us/topic/fix-problems-that-block-programs-from-being-installed-or-removed-cca7d1b6-65a9-3d98-426b-e9f927e1eb4d)
- Try [Microsoft's documentation for these issues](https://support.microsoft.com/en-us/topic/how-to-troubleshoot-windows-installer-errors-dc2f66aa-2ae2-1e61-6104-b8166628fbde)
- [`DISM` and `sfc`](https://support.microsoft.com/en-us/topic/use-the-system-file-checker-tool-to-repair-missing-or-corrupted-system-files-79aa86cb-ca52-166a-92a3-966e85d4094e)
- Unfortunately, if these don't resolve these issue, you may need to reinstall Windows; these problems are a problem with a Windows component, not OpenKneeboard

## Unsupported alternative

You can use tools like 7-zip to extract the MSI.

- No help/support is available if you do this
- Check for and remove any old versions in add/remove programs first
- Check for and remove any old versions in my [OpenXR API Layers tool](https://github.com/fredemmott/OpenXR-API-Layers-GUI/releases/latest) first
- You will need to turn on OpenXR support in Settings -> VR
- You are likely to encounter issues with some applications/games from the Microsoft Store, including:
  - OpenXR Tools for Windows Mixed Reality
  - some editions of Microsoft Flight Simulator (2020)