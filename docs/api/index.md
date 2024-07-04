---
title: APIs
has_children: true
---

OpenKneeboard can be controlled from:

- [C](c.md), and other languages via FFI, such as [Python](python.md)
- [Lua](lua.md)

For other language, using the C API is strongly recommended; see the documentation for your language/framework for how to call C libraries.

If you are not comfortable calling C functions, the [remote control executables](../features/remote-controls.md) provide a more limited API. You can locate them with:
- `..\utilities\` from the [`InstallationBinPath`](registry-values.md#installationbinpath) registry value on v1.8.4 and above
- `%PROGRAMFILES%\OpenKneeboard\utilities` on earlier versions (deprecated)