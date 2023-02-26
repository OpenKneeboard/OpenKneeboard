---
parent: APIs
---

# C API

## Overview

**The C API will be available in OpenKneeboard v1.4**. There is no C API in OpenKneeboard v1.3 or below.

The C API is able to send information, events, or requests to OpenKneeboard; it is not able to receive data from OpenKneeboard. It requires a dynamic library - `OpenKneeboard_CAPI32.dll` or `OpenKneeboard_CAPI64.dll`. The implementation of this DLL changes in every version of OpenKneeboard, however the API is expected to remain stable.

 - The DLL is installed and kept up to date in `C:\Program Files\OpenKneeboard\bin\`
 - The header is available in `C:\Program Files\OpenKneeboard\include\`

 As the implementation changes with every release, you should dynamically load the DLL using `LoadLibraryW()`, and find the function of interest with `GetProcAddress()`.

I recommend that programs attempt to locate the DLL by:
- checking for the presence of an `OPENKNEEBOARD_CAPI_DLL` environment variable; if present, use it as the full path to the DLL
- otherwise, check `%ProgramFiles%\OpenKneeboard\bin\`. Program files should ideally be located with `SHGetKnownFolderPath()`, but the `ProgramFiles` environment variable can also be used when `SHGetKnownFolderPath()` is impractical

## Functions

```C
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
- [Python](https://github.com/OpenKneeboard/OpenKneeboard/blob/master/src/utilities/capi-test.py)
