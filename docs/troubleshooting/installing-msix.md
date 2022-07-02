# Troubleshooting MSIX Installation Issues

## "How do you want to open this file?"

Install [App Installer from the Microsoft Store](https://www.microsoft.com/en-us/p/app-installer/9nblggh4nns1); this is usually installed automatically.

## Stuck at "Installing Required Frameworks" (usually 15% or 16%)

These may just be slow, depending on the speed of your internet connection; you can check for progress in the Windows notification area from the 'message' icon next to the Windows clock.

If it seems to be making no progress, you can:

1. Cancel the installation
2. From powershell: `get-appxpackage -name '*AppRuntime*' | remove-appxpackage`
3. If you get errors saying that a package can't be removed because another package depends on it, uninstall the other package, then repeat the powershell command
4. Install the frameworks below
5. Install OpenKneeboard

### Frameworks for OpenKneeboard v1.1 and above

* [Windows App Runtime v1.1](https://aka.ms/windowsappsdk/1.1/1.1.1/windowsappruntimeinstall-1.1.1-x64.exe) - [alternative (see below)](https://aka.ms/windowsappsdk/1.1/1.1.1/windowsappruntimeredist-1.1.1.zip)

### Frameworks for OpenKneeboard v1.0 and below

* [`Microsoft.VCLibs.x64.14.00.Desktop.appx`](https://docs.microsoft.com/en-us/troubleshoot/developer/visualstudio/cpp/libraries/c-runtime-packages-desktop-bridge)
* [Windows App Runtime v1.0](https://aka.ms/windowsappsdk/1.0/1.0.4/windowsappruntimeinstall-1.0.4-x64.exe) - [alternative (see below)](https://aka.ms/windowsappsdk/1.0/1.0.4/windowsappruntimeredist-1.0.4.zip)

### Alternative installation of Windows App Runtime

If the powershell + exe approach doesn't work for you, you can download zips
instead from the alternative links.

To install these, extract them, then install every `.msix` inside the
`MSIX\win10-x64` subfolder; you may get an error for some of them - this is
fine, ignore them.
