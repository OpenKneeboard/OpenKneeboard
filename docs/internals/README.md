---
title: Internals
has_children: true
---

# OpenKneeboard Internals

## Overview

OpenKneeboard is a C++23 app built using CMake; it has 3 main components:

* the main Win32 GUI app: primarily intended for configuration, and can be used
  as a non-VR view.
* 'injectables': OpenKneeboard does several things that there is no API for,
  such as rendering an overlay (unless using SteamVR), and accessing graphics
  tablets when another process (such as the game) owns the active window. To
  provide these features, OpenKneeboard injects DLLs into other processes.
* a DCS Lua script and corresponding DLL: exports data from DCS World, such as
  the current mission, theater, and aircraft

The C API example targets C++17 in order to be accessible to a wider audience.

OpenKneeboard currently only targets Visual Studio 2022 - not clang - due to limitations in the WinUI3 components - however it aims to be clean with `clang-tidy`. Specifically, OpenKneeboard aims to be buildable both with the latest version of Visual Studio 2022, and whichever version is currently in the GitHub Actions runner, which is often a few releases behind. As of 2024-07-07, this compilers' C++23 support is not complete, but does include some useful features over C++20, especially in the standard library.

## Building

Visual Studio 2022 must be installed; the free 'community' edition is fine. Make sure that support for 'CMake Presets' is enabled

If you use VSCode, just open the folder and VSCode should prompt to install CMake support if necessary. Then, just click 'build'
If you use CLion, open the folder as a project, and everything should 'just work'.

Alternatively, from a developer command prompt:

```
mkdir build
cd build
cmake .. --preset vcpkg
cmake --build . --config Debug --parallel
```

The 'developer command prompt' is included with Visual Studio, and makes cmake and the compilers available.

After building, the executable will be in `build/src/app/app-winui3/Debug`; the various utilities (including the test viewer) will be in `build/src/utilities/Debug`.

If you are distributing OpenKneeboard or a derivative work, please change the following CMake variables to avoid interoperability issues:

- `PROJECT_REVERSE_DOMAIN`: e.g. `com.yourdomain.yourprojectname` or `io.github.yourusername.yourprojectname`; used for IPC paths/keys
- `PROJECT_OPENXR_API_LAYER_NAME`: e.g. `XR_APILAYER_FREDEMMOTT_OpenKneeboard`; replace both the project name and vendor ID. You can register your own OpenXR vendor ID, or `NOVENDOR` is a common alternative.
- `PROJECT_OPENXR_API_LAYER_DESCRIPTION`: a short, human-readable description of your project.

To avoid user confusion or violating OpenKneeboard's license, you should avoid using the word 'OpenKneeboard' or `XR_APILAYER_FREDEMMOTT` in the name or description of your project. Read the license for details on your obligation to remove names and unique identifiers when distributing binaries.

Despite these options, OpenKneeboard is not an SDK, and no support is available for building other projects using it.

## IPC

### OpenKneeboard app -> renderers

The main app renders the kneeboard to shared buffers
(`ID3D11Texture2D`/`IDXGISurface`), and maintains the other necessary data for
renderers in shared memory. This information includes:
* user preferences like render coordinates and size.
* necessary implementation details, such as which shared texture is currently
  in use and the region of the texture that is active.
* a sequence number to easily identify when there is a new frame available.

For details, see:

* `OpenKneeboard::SHM::Config`
* `OpenKneeboard::SHM::Header`
* `OpenKneeboard::SHM::Reader` and `OpenKneeboard::SHM::Writer`
* the `test-viewer` and `test-feeder` utilities

### Games -> OpenKneeboard app

This is used for events like "mission loaded", passing data like mission file,
current aircraft, and current theater.

The `OpenKneeboard::APIEvent` class should be used for this. Internally, the
`OpenKneeboard::APIEvent` class uses
[Mailslots](https://docs.microsoft.com/en-us/windows/win32/ipc/mailslots), which can be thought of as similar to UDP, or POSIX or System V message queues.

Unlike socket-based approaches, there is no need to obtain an unused port, communicate that port number to the game, or set firewall rules. Essentially,
mailslots allow zero-config one-way communication without connection management.

## The main app: `src/app/`

This is a WinUI3 app; it has several functions:
- provide a configuration UI
- provide an out-of-game preview
- directly function as a kneeboard on a non-VR multi-monitor setup
- watch for configured game processes, and load injectable DLLs into them as needed
- render the kneeboard to a DXGI shared texture for injectable renderers to use
- act as a SteamVR overlay

`src/app/app-common` contains UI-agnostic components; `src/app/app-winui3` contains
the UI. These may be merged in the future - this structure exists because there
was previously also an `app-wx` using wxWidgets.

### Tab Types: `src/app/tabs`

A tab is responsible for loading and drawing its' content (including graphics
tablet drawings), serializing its' settings as JSON, restoring settings
from JSON, and producing a settings UI.

By default, any pen input to an `OpenKneeboard::ITab` will be treated as a
drawing/annotation; you can replace or extend this behavior by overriding
the `OnCursorEvent` method.

To make a new tab type available in Setttings -> Tabs, add it to the
`OPENKNEEBOARD_TAB_TYPES` macro in `src/app/okTabsList/TabTypes.h`.

### Rendering

Kneeboard data is rendered with Direct2D - see
[why-Direct2D.md](why-Direct2D.md) - to DXGI surfaces; these are currently
D3D11 textures.

The `okTabCanvas` widget maintains a swap chain (set of buffers) for rendering
to the main app window, and when needed by the app, instructs tabs to render
to one of the back buffers, then presents it to the window.

The `InterprocessRenderer` class maintains a single 'canvas' buffer, and multiple shared buffers that are accessible by other processses (and injectables in those processes). It instructs tabs to render to the canvas buffer, and when
complete, it copies the canvas to the next shared buffer, and updates the
shared memory configuration to let renderers know there is a new frame.

All objects involved in rendering use the same DirectX device/factory/context
instances, shared by passing an `OpenKneeboard::DXResources` struct to their
constructors. This allows all render paths (to screen and to shared texture) to
share resources like brushes and bitmaps, reducing CPU and GPU memory usage,
and the need for CPU `<->` GPU and GPU `<->` GPU copies.

The primary requirement is that the same physical GPU is used for both paths,
however several resource types check that they were created from the same
object instance as the device context/render target, so we need to pass
the exact same `ID3D11Device`, `IDXGIDevice`, `ID2D1Device`, `ID2D1DeviceContext` etc.

## Injectables: `src/injectables`

These are used for:

* accessing graphics tablet events in the background
* rendering the overlay for non-VR games, and games using the oculus SDK

See [injectables.md](injectables.md) for details.

## DCS Hook: `src/dcs-hook`

A C++ DLL Lua extension (`OpenKneeboard_LuaAPI.cpp`/`.dll`) is used to give
DCS Lua scripts the ability to send `OpenKneeboard::APIEvent`s via the
mailslot. `OpenKneeboardDCSExt.lua` loads the DLL, and uses the usual DCS
Lua APIs to observe events, and passes them to the DLL which in turn passes
them on to `OpenKneeboard`.

If possible, new features should be added to the Lua code without extending
the functionality of the Dll: this is part of a general principle of doing as
little as possible in native code in other processes.

The available Lua API is documented in the `API` subdirectory of your DCS
World installation.

## Games: `src/games`

Automatic configuration information for known games.

## Libraries: `src/lib`

This is generally stuff that is needed by both the main app, and by injectables
or utilitites. This includes `OpenKneeboard::APIEvent`, `OpenKneeboard::SHM`,
and various smaller components.

## Utilities: `src/utilities`

These are tools intended to help develop or debug OpenKneeboard:

* `fake-dcs`: send DCS API events to the main OpenKneeboard app, populating
  the radio log, theater, and aircraft kneeboard tabs.
* `test-apievent-feeder`: feed fake radio log messages to OpenKneeboard once
  a second. Useful to test the APIEvent code, and that it handles the app
  not running, or being closed, or being restarted.
* `test-feeder`: populate the shared memory and textures with a cycling test
  pattern, without the OpenKneeboard app running.
* `test-viewer`: display the contents of the shared memory and textures.

## General Principles

It is **much** better for OpenKneeboard to crash or for OpenKneeboard's
Lua to silently fail - in which case, the DCS-specific Kneeboard tabs stop
automatically updating - than for OpenKneeboard's injectables to crash another
process - especially if someone's hours into a flight/mission.

As of writing there are no known crashes, but the idea is to structure code so
that there is as little risk to the game processes:

* Do as little as possible in other processes: e.g. prefer to put code in the 
  OpenKneeboard app than in injectables
* Do as little as possible in DLLs in other processes: e.g. if you need more
  data from DCS, put as much code as possible in the Lua code rather than in
  the dll
