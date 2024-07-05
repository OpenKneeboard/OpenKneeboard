---
parent: APIs
---

# C API

## Overview

The C API is able to send information, events, or requests to OpenKneeboard; it is not able to receive data from OpenKneeboard. It requires a dynamic library - `OpenKneeboard_CAPI32.dll` or `OpenKneeboard_CAPI64.dll`. The implementation of this DLL changes in every version of OpenKneeboard, however the API is expected to remain stable.

 - The DLL is installed and kept up to date in `C:\Program Files\OpenKneeboard\bin\`
 - The header is available in `C:\Program Files\OpenKneeboard\include\`

 As the implementation changes with every release, you should dynamically load the DLL using `LoadLibraryW()`, and find the function of interest with `GetProcAddress()`, or equivalent features in your preferred programming language.

## Locating the DLL

I recommend that programs attempt to locate the DLL by:

1. Check for the presence of an `OPENKNEEBOARD_CAPI_DLL` environment variable; if present, use it as the full path to the DLL.
2. If that fails, check for the [InstallationBinPath](registry-values.md#installationbinpath) registry value; this is set when OpenKneeboard launches in v1.8.4 and above
3. If that fails, check `%ProgramFiles%\OpenKneeboard\bin\`. Program files should ideally be located with `SHGetKnownFolderPath()` or equivalent (e.g. `Environment.GetFolderPath()` in .Net), but the `ProgramFiles` environment variable can also be used

## Functions

`OpenKneeboard_send_utf8()` is recommended for all apps, except those that are already heavily using Win32 'wide' APIs. The message name and value **must** be in UTF-8, not the current system code page.

```c
OPENKNEEBOARD_CAPI void OpenKneeboard_send_utf8(
  const char* messageName,
  size_t messageNameByteCount,
  const char* messageValue,
  size_t messageValueByteCount);

OPENKNEEBOARD_CAPI void OpenKneeboard_send_wchar_ptr(
  const wchar_t* messageName,
  size_t messageNameCharCount,
  const wchar_t* messageValue,
  size_t messageValueCharCount);

#define OPENKNEEBOARD_CAPI_DLL_NAME_A /* varies */
#define OPENKNEEBOARD_CAPI_DLL_NAME_W /* varies */
```

- `OPENKNEEBOARD_CAPI_DLL_NAME_A` will be the filename as a C string literal, e.g. `"OpenKneeboard_CAPI64.dll"` or `"OpenKneeboard_CAPI32.dll"`
- `OPENKNEEBOARD_CAPI_DLL_NAME_W` will be the filename as a C wide-string literal, e.g. `L"OpenKneeboard_CAPI64.dll"` or `L"OpenKneeboard_CAPI32.dll"`

## Messages

See [the messages documentation](messages.md) for information on supported messages.

## Examples

- [C++](https://github.com/OpenKneeboard/OpenKneeboard/blob/master/src/utilities/capi-test.cpp)
- [C#](https://gist.github.com/fredemmott/7a4d4f8584c7f217977fd39cebc98dba)
- [Python](https://github.com/OpenKneeboard/OpenKneeboard/blob/master/src/utilities/capi-test.py)
