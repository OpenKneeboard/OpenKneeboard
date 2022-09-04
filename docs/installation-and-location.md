# Installation

## Location

OpenKneeboard - and any other application installed from MSIX, AppX, or the Microsoft Store - is installed into `C:\Program Files\WindowsApps`. Windows locks down this folder, and while it's possible to bypass Window's restrictions, this tends to break things and is not recommended.

Because of these restrictions, OpenKneeboard copies some helpers into `C:\ProgramData\OpenKneeboard`

## Starting OpenKneeboard

Once OpenKneeboard is installed, it can be started:

- from the Start menu
- as `openkneeboard` or `openkneeboard.exe` from a terminal, the run dialog, or other applications
- via `C:\ProgramData\OpenKneeboard\OpenKneeboard-Launch-WindowsApp.exe` if a full path is needed

## Actual executable path

**Launching OpenKneeboard's real .exe directly will usually not work and is not supported**; the Windows App system is **required**.

If you need the path of the real exe for a specialized reason, e.g. for HidHide:

1. Open 'Windows PowerShell' from the Start menu
2. Run `"$((Get-AppxPackage -name '*OpenKneeboard*').InstallLocation)\bin\OpenKneeboardApp.exe"`
3. This will show you the full path to the exe; this path will change with every upgrade

## Uninstalling OpenKneeboard

OpenKneeboard can be uninstalled from `Add/Remove Programs`; because of limitations of the MSIX format, this will leave the files in `C:\ProgramData\OpenKneeboard` and the OpenXR registry settings, however these are almost always harmless and do nothing when OpenKneeboard is not installed.

To remove these remaining components from v1.2 or above, download the 'Utilities' zip from [the release page](https://github.com/OpenKneeboard/OpenKneeboard/releases), and run `OpenKneeboard-Uninstall-Helper.exe` from that zip. For older versions:

1. Delete `C:\ProgramData\OpenKneeboard`
2. Open `regedit`
3. Delete any references to OpenKneeboard in `Computer\HKEY_CURRENT_USER\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit` by right-clicking on the row and selecting 'Delete'
4. Delete any references to OpenKneeboard in `Computer\HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit` the same way
