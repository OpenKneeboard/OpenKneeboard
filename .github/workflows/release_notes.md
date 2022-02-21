# Before installing: this might not be the latest release - [you can always get the latest release here](https://github.com/fredemmott/OpenKneeboard/releases/latest)

# You probably want `OpenKneeboard-@TAG@.msix`

See [docs/downloadable-files.md](https://github.com/fredemmott/OpenKneeboard/blob/@TAG@/docs/downloadable-files.md) for more information on the available files.

Please start with [the README](https://github.com/fredemmott/OpenKneeboard/blob/@TAG@/README.md); there is also extra documentation for:

- [Huion tablet users](https://github.com/fredemmott/OpenKneeboard/blob/@TAG@/docs/huion.md)
- [StreamDeck users](https://github.com/fredemmott/OpenKneeboard/blob/@TAG@/docs/streamdeck.md)
- [Wacom tablet users](https://github.com/fredemmott/OpenKneeboard/blob/@TAG@/docs/wacom.md)

If you need help, see [Getting Help](https://github.com/fredemmott/OpenKneeboard#getting-help) in the README.

The installer says that OpenKneeboard "Uses all system resources"; this means that it is a traditional windows application, not a sandboxed Windows Store application. This is required for Oculus, Non-VR, and WinTab support, as they require changing the behavior of the game processes, which are not inside the sandbox. For technical details, see [Microsoft's documentation](https://docs.microsoft.com/en-us/windows/uwp/packaging/app-capability-declarations) for the `runFullTrust` capability. OpenKneeboard does not run as administrator or with elevated privileges.
