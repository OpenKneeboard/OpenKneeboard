# Troubleshooting MSIX Installation Issues

## "How do you want to open this file?"

Install [App Installer from the Microsoft Store](https://www.microsoft.com/en-us/p/app-installer/9nblggh4nns1); this is usually installed automatically.

## Stuck at "Installing Required Frameworks" (usually 15% or 16%)

These may just be slow, depending on the speed of your internet connection; you can check for progress in the Windows notification area from the 'message' icon next to the Windows clock.

If it seems to be making no progress, you can also download these direct from Microsoft:

* [`Microsoft.VCLibs.x64.14.00.Desktop.appx`](https://docs.microsoft.com/en-us/troubleshoot/developer/visualstudio/cpp/libraries/c-runtime-packages-desktop-bridge)
* [Windows App SDK](https://docs.microsoft.com/en-us/windows/apps/windows-app-sdk/downloads) - scroll down to 'Releases' and download "Installer (x64)" from the release marked '(Latest)' - ignore any preview releases.
