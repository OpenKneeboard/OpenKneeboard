# C API

## Overview

**The C API will be available in OpenKneeboard v1.4**

The C API is able to send information, events, or requests to OpenKneeboard; it is not able to receive data from OpenKneeboard. It requires a dynamic library - `OpenKneeboard_CAPI.dll`. The implementation of this DLL changes in every version of OpenKneeboard, however the API is expected to remain stable.

 - The DLL is installed and kept up to date in `C:\Program Files\OpenKneeboard\bin\OpenKneeboard_CAPI.dll`
 - The header is available in `C:\Program Files\OpenKneeboard\include\`

 Only a 64-bit applications are supported.

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
```

## Messages

See [the messages documentation](messages.md) for information on supported messages.

## Examples

- [C++](../../src/utilities/capi-test.cpp)
- [Python](../../src/utilities/capi-test.py)
