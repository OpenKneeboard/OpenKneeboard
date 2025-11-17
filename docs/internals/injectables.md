---
parent: Internals
---

# Injectables

OpenVR (SteamVR) has built-in support for 3rd-party overlays - like
OpenKneeboard - but none of the other supported APIs do.

Applications using these APIs are supported by injecting a DLL into them which
hooks/intercepts calls from the application to these APIs (e.g. Direct3D,
OpenXR), and adding the required functionality into the application itself at
runtime. Essentially, the purpose of these DLLs is to copy an image out of
shared memory (SHM), and render it as part of the application's normal rendering
pipeline.

Popular 2D overlays like the Steam Overlay and Discord's Game Overlay have the
same problem, and essentially do the same thing: inject a DLL, and modify the
game's behavior so that the game renders the overlay.

## Special Injectables

The vast majority of the injected DLLs in OpenKneeboard are renderers, however
there are a small number of special ones:

### OpenKneeboard-AutoDetect

This is the default injectable. It intercepts every known API to detect which are in use. Once it's received 30 frames, it will load an actual implementation if there is one.

This is also the only injectable that hooks OpenVR/SteamVR: if it
detects SteamVR is in use, it will will not load any implementation, as the main OpenKneeboard application should be using the OpenVR overlay APIs.

For example, if it detects Oculus and Direct3D 11 in use, it will load `OpenKneeboard-oculus-d3d11`; if it detects Direct3D 11, but does not detect any other APIs, it will load `OpenKneeboard-nonvr-d3d11`.

If both Direct3D 11 and Direct3D 12 are detected, Direct3D 12 takes priority: it is likely that the game is a Direct3D 12 application using https://docs.microsoft.com/en-us/windows/win32/direct3d12/direct3d-11-on-12

## Developer Workflow

While eventually, implementations must be tested with real games and the main
app, this is a bit slow and awkward while developing/debugging. My
recommendations are:

- do not run the main OpenKneeboard application when working on a new injectables.
- use https://github.com/nefarius/Injector to load/unload specific DLLs
  explicitly.
- for Oculus, the various `OculusRoomTiny` examples from the SDK instead of
  real games, as D3D11, D3D12, OpenGL, and Vulkan are all supported.
- those can also be used to test the non-VR injectables: the kneeboard should
  appear on the desktop mirror window.
- for OpenXR, the [hello_xr](https://github.com/KhronosGroup/OpenXR-SDK-Source/tree/master/src/tests/hello_xr)
  test is useful, as it also supports D3D11, D3D12, OpenGL, and Vulkan.
- use the `test-feeder` utility to put valid demo data into SHM, without the
  added complications that autoinjection may add.
- when developing, run the target applications in a debugger, e.g. https://www.microsoft.com/en-us/p/windbg-preview/9pgjgd53tn86

**Exception**: while it is generally more efficient not to use the
OpenKneeboard app, there often issues that only occur if the DLL is loaded
very shortly after the target application/game starts. The OpenKneeboard app
autoinjection is very useful for testing this, as it tends to inject within
10s of milliseconds.

## Interception approaches

There are two ways we can do this:
- if we have a singleton C++ or COM object, we can use `VirtualProtect()` to
  make the memory writable, swap the vtable entry, and finally use
  `VirtualProtect()` to restore the previous memory settings. This approach
  is used for OpenVR's `IVRCompositor`, and will likely be used for
  OpenXR in the future.
- Detours: replace the first few instructions in the target function to call
  our code. [Microsoft Detours](https://github.com/microsoft/Detours)
  is used for the low-level implementation, and OpenKneeboard includes
  [additional abstractions and conventions](detours-and-hooks.md) to address
  multithreading issues.

## Getting an in-use instance of a class

1. find a function that will be called frequently (usually every frame), and
   is either passed the object you want, or an object that has access to the
   object you want
2. intercept that function

For example, the D3D11 injectables need the `ID3D11Device` that the game
is using. All D3D11 apps call `IDXGISwapChain::Present`, and as an
`IDXGISwapChain` is an `IDXGIDeviceSubObject`, we can call `this_->GetDevice()`
in our detour.

## Finding virtual methods

Virtual methods need to be read from the vtable.

* get an instance of the object; if you can get an in-use instance, that's
  great, but otherwise you can usually create a second, dummy interface, read
  the vtable, then immediately discard it
* if the class/interface is a COM interface, you can directly read the vtable
  in C (not C++) code; e.g. the address of `IDXGISwapChain::Present` is
  available in `swapChain->lpVtbl->Present`
* otherwise, you need to find the vtable offset, and read the vtable directly

If you need to read the vtable directly, unless there is inheritance involved,
the object will start with a pointer to the vtable; if inheritance is involved,
you need to dig into the ABIs to fully resolve them.

Instead of hardcoding offsets, it's more readable and maintainable to
reconstruct the vtable - or a partial vtable. For example, while we could hardcode
that the address of `IVRCompositor::WaitGetPoses()` is at byte 17 (or `void*`
offset 2), instead, we cast the VTable to:

```C++
struct IVRCompositor_VTable {
  void* mSetTrackingSpace; // unused
  void* mGetTrackingSpace; // unused
  void* mWaitGetPoses; // stop here as it's the only one we need for now
};
```

This matches the declaration order of virtual functions in the header file,
and can also be extended to cleanly deal with inheritance by mirroring
the inheritance in the vtable structs.

## Complication: other overlays

As this is essentially the same problem - and same solution - that other
overlays face, they can interfere.

For example, both OpenKneeboard-nonvr-d3d11 and the Steam Overlay need to
intercept calls to `IDXGISwapChain::Present`, however the Steam Overlay -code
gets very unhappy if we do it first - or if we replace their modification.

Instead, we detect if the Steam Overlay is present, and if so, instead of
intercepting `IDXGISwapChain::Present`, we intercept Steam's interception of
`IDXGISwapChain::Present`. It is helpful that Steam always launches the process
with the overlay DLL loaded - we do not need to worry about it potentially
being loaded at a later time.

Similar logic will likely be needed for compatibility with other overlays in
the future.

## Complication: non-exported functions

Intercepting a function requires knowing its' location in memory - however,
sometimes we need to intercept functions that were not exported, i.e. were not
intended to be referenced from outside the module (DLL) they are defined in.

One example is intercepting the Steam Overlay's version of
`IDXGISwapChain::Present`: it was not intended that any other code would need
to refer to that function - however, we need to, in order to interoperate with
it.

This requires a fairly nasty approach: pattern matching on compiled code.

Fortunately, as we're looking for the address of the start of a function,
there's a few tricks we can use to make this faster and more reliable.

There are two important facts:

- we are looking for the start of a function
- we know that function is compatible with `IDXGISwapChain::Present`

This in turn tells us:

- the function signature.
- the calling convention.
- the address we're looking for must be aligned on a 16-byte boundary (on x64)

In the case of the Steam Overlay's `IDXGISwapChain::Present` function, this
knowledge is sufficient (as of 2022-01-21) to unambiguously identify the
relevant function: it is the only function in the DLL that:

- follows the `__stdcall` calling convention - which tells us how `this`
  and the arguments are passed to the function: on the stack vs registers,
  which registers, etc.
- takes 3 arguments that are 64-bits-wide or smaller. `this` is the implicit
  first argument in C++ `__stdcall` method calls on x64.

The native code to unmarshal the parameters for a function like this is currently
unique in the DLL, so is sufficient without even looking at the function body.

In this case, I also match the start of the function body, as it seems unlikely
to change, and reduces the risk of accidentally matching some other function
in the future. The start of the function body seems unlikely to change in this
case as it is functionally equivalent to:

```C++
if ((Flags & DXGI_PRESENT_TEST)) {
  // Don't render the Steam Overlay when there's not actually a frame
  // being rendered, e.g. when switching from the idle state
  return original_DXGISwapChain_Present(this_, SyncInterval, Flags);
}
```

The first few instructions in the body test the if condition, and we match
on those; we don't match on the body of the if statement.
