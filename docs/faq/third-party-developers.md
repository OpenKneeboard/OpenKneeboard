---
parent: FAQ
title: Third-Party Developers
---

# Third-Party Developer FAQ
{: .no_toc }

If you're a developer and your OpenKneeboard-related question is not answered here, please say hello in `#code-talk` [on Discord](https://go.openkneeboard.com/discord).

## Table of Contents
{: .no_toc, .text-delta }

1. TOC
{:toc}

## How do I use OpenKneeboard to show content from my app?

For commercial enquiries, please [contact Fred Emmott Software Services LLC via email](mailto:fred@fredemmott.com]. I do not offer any direct-to-consumer services or support via email.

You have three options:
- implement a custom tab type in OpenKneeboard with C++ and Direct3D 11
- provide a file or web page, and instruct users on how to configure OpenKneeboard to access that content. OpenKneeboard will automatically reload files when they are modified.
- provide a window, and instruct users on how to configure a Window Capture tab for your app

OpenKneeboard has additional integration APIs available for [web dashboards](../api/web-dashboards.md); however, text quality will be higher for text or PDF files.

Do not write software that changes OpenKneeboard's configuration files; it is *extremely* likely to break users' configuration when they update OpenKneeboard.

OpenKneeboard is a tool for users to show their content how they wish in VR, via OpenKneeboard's settings. It is not a developer toolkit.

Feel free to *read* OpenKneeboard's configuration files from your software, but keep in mind there are no attempts at providing stability.

## Why can't my window be captured?

As if OBS Studio can capture your window with the 'Windows 10' method *when not elevated*, OpenKneeboard generally can; if this is not the case, please open a GitHub issue.

The most common reasons for this failing are:
- as OpenKneeboard does not support running elevated, it is not able to capture windows that belong to an elevated process
- `Windows::Graphics::Capture` has undocumented limitations on window styles; in practical terms, your window is usually required to appear in the task bar
  - if this is unwanted, consider adding an option for OpenKneeboard users and streamers
  - if you are using the windows API directly, `WS_EX_TOOLWINDOW` removes items from the task bar

## How do I support transparency?

- for HTML files or web pages, use CSS; Chromium is used, which includes support for `background-color: transparent;` and `rgba(...)`, including on the `body` tag.
- for capturable Windows, Windows only supports composable RGBA via DirectComposition. If you are already using a DXGI swapchain, moving to a DirectComposition swapchain with `DXGI_ALPHA_MODE_PREMULTIPLIED` is [relatively straightforward](https://github.com/OpenKneeboard/OpenKneeboard/commit/694b87570f90d2d68ea9bcc596afc0cdefc23430). All other approaches resolve transparency within the app and provide Windows with RGB data, so the capture will not include transparency.

If you are using a framework that manages your window/swapchain/transparency, see the documentation and support resources for that framework.

If your application is written in JavaScript or similar but the app framework you are using does not support true RGBA windows, consider making the app available as a web page too (e.g. on a localhost server); this is commonly offered as an 'OBS URL' or 'overlay URL', or similar.

## How can people capture my app without the title bar?

Users must turn on the 'client area only' option in OpenKneeboard's tab settings; this uses the Win32 `GetClientRect()` function.

If your app/framework does not support `GetClientRect()`, you can add support to your app by implementing `WM_NCCALCSIZE`; if your window position is fully specified, you will need to call `SetWindowPos()` with `SWP_FRAMECHANGED` parameter to trigger this message.

## I haven't yet built an app; how do I get content into OpenKneeboard?

For commercial enquiries, please [contact Fred Emmott Software Services LLC via email](mailto:fred@fredemmott.com]. I do not offer any direct-to-consumer services or support via email.

The best way is to implement a new tab type in C++ with Direct3D11/Direct2D/DirectWrite.

That is not particularly friendly, so an alternative is to implement your content as an HTML page - OpenKneeboard offers [additional web APIs](../api/web-dashboards.md). If you need to go beyond what JavaScript enables, consider making your app a webserver, like [BMS Kneeboard Server](https://github.com/AviiNL/bms-kneeboard-server).

I do not recommend building a separate GUI app and using Window Capture: there are too many frameworks/apps for me to consider building an API for good integration, e.g:
- this approach does not support pages/bookmarks/table of contents
- GUI apps can *only* be controlled in game via tablet -> mouse emulation, unless you build your own input system that runs independently. OpenKneeboard's standard next/previous page/bookmark bindings etc will not work
- changes to the GUI framework may break mouse emulation in an update, entirely outside of your control; if that happens, [you'll need to fix it](#why-doesnt-the-mouse-emulation-work-correctly-in-my-app-when-using-window-capture-tabs)

There are no particular GUI frameworks I recommend, but I recommend finding an example app in the same framework and making sure that mouse injection works well enough for your purposes.

## How do I choose where my app's content is displayed in VR?

Instruct users on how to change their user settings to your recommendations.

OpenKneeboard is a tool for users to show their content how they wish in VR, via OpenKneeboard's settings. It is not a developer toolkit.

Do not write software that changes OpenKneeboard's configuration files; it is *extremely* likely to break users' configuration when they update OpenKneeboard.

## How do I use OpenKneeboard to create my own OpenXR overlay?

For commercial enquiries, please [contact Fred Emmott Software Services LLC via email](mailto:fred@fredemmott.com]. I do not offer any direct-to-consumer services or support via email.

OpenKneeboard is a tool for users to show their content how they wish in VR, via OpenKneeboard's settings. It is not a developer toolkit. This is not a supported use-case of OpenKneeboard.

You are welcome to fork and follow the terms of the license to create a new project, however I am unable to provide any assistance to third-party projects.

Alternatively, if you don't need to handle input, it is relatively straightforward to create your own OpenXR overlay from scratch:

1. create an OpenXR API layer
2. in your layer, implement the `xrEndFrame()` function
3. Append additional composition layers to the layer submitted in `XrFrameEndInfo` struct

Your code will need to handle copying your overlay into the composition layer swapchains; this will require basic knowledge of the graphics APIs you care about, e.g. Direct3D 11, Direct3D 12, Vulkan. If you are not familiar with graphics APIs, the official Vulkan tutorial and various 'hello triangle' tutorials are sufficient. You may find Microsoft's [DirectXTK](https://github.com/microsoft/DirectXTK) and [DirectXTK12](https://github.com/microsoft/DirectXTK12) projects useful. If you are not familiar with 3D math for positioning your layer, DirectXTK's SimpleMath component can be useful, or the [`xr_linear.h` header from the OpenXR SDK](https://github.com/KhronosGroup/OpenXR-SDK/blob/main/src/common/xr_linear.h).

## How do I control OpenKneeboard from my app?

Keep in mind the purpose of OpenKneeboard: OpenKneeboard is a tool for users to show their content how they wish in VR, via OpenKneeboard's settings. It is not a developer toolkit.

With that in mind, take a look at [the API documentation](https://openkneeboard.com/api/); if your feature requires an additional API and fits with the intended purposes of OpenKneeboard, open a feature request or pull request for an additional API.

Do not write software that changes OpenKneeboard's configuration files; it is *extremely* likely to break users' configuration when they update OpenKneeboard.

## Why doesn't the mouse emulation work correctly in my app when using Window Capture tabs?

Windows and app development frameworks try very hard in many different ways to make sure that the mouse cursor only affects the foreground window. OpenKneeboard tries to work around this by sending events itself, and intercepting Win32 functions such as `GetForegroundWindow()`. Some frameworks and apps use mechanisms that OpenKneeboard does not intercept, or OpenKneeboard's implementation does not fool the app.

These issues are extremely time consuming, and there are too many different frameworks and apps for me to investigate them, but contributions are very welcome - however, this is an advanced topic.

To directly fix the injection, you will need familiarity with:
- C++
- the Win32 API
- window messages and how they work (e.g. WM_MOUSEMOVE) and related APIs (e.g. RawInput, HID)
- how your app and/or framework handles mouse events
  - alternatively, if you use a source-available GUI framework, a willingness to dig into your frameworks' source code
  - alternatively, if you use a closed-source GUI framework, a willingness to dig into how your framework works via reverse engineering tools. For .Net there's ILSpy (for .Net), and for native code there's Ghidra or IDA

It is easy for OpenKneeboard to send additional or different window messages, and to intercept additional functions; the hard part is figuring out what messages and functions, and what the modified behavior should be.

If you are familiar with the various components except for OpenKneeboard, please reach out in `#code-help` [on Discord](https://go.openkneeboard.com/discord).

### An alternative approach

OpenKneeboard could directly pass your application its' own raw cursor events, and leave it up to your app to deal with them. Your app might then to choose to inject them into the GUI framework itself (usually via accessibility or testing APIs), or just do its' own thing with the events.

This would be via extensions to [the C API](../api/c.md), so you would need to be familiar with how to call C functions from your programming language of choice - some examples are at the bottom of the page.

If this approach is interesting to you, please reach out in `#code-help` [on Discord](https://go.openkneeboard.com/discord); it is not currently planned, and is unlikely to be implemented unless there are signs of interest.

## API layers that manipulate poses

### What should the API layer order be?

First, see [the user FAQ](index.md#i-use-another-openxr-tool-api-layer-what-should-the-order-be); like other API layers, pose manipulation layers should be closer to the runtime and farther from the game than OpenKneeboard, as:

- it would be valid for the game itself to also do the same things that OpenKneeboard does
- pose manipulation layers generally want to manipulate OpenKneeboard's world-locked content the same way as they manipulate the game world, along with any world-locked content the game produces (e.g. HUDs and menus are world-locked overlays in some games)
- OpenKneeboard does not modify the games's API calls, except for appending a quad layer in `xrEndFrame()`; it does however make several additional, *independent* API calls, in particular, `xrLocateSpace()`, and `xrCreateReferenceSpace()`

If OpenKneeboard is closer to the runtime than your layer, OpenKneeboard's world-locked content should be locked to the real-world instead of your modified world space, which is generally undesirable, but otherwise everything should work fine.

If your pose manipulation layer needs to be between the game and OpenKneeboard, your layer most likely has incorrect behavior, and will also be broken for some games: OpenKneeboard does nothing that the game itself is not allowed to do, some games do the same things, and it is not possible for your layer to be closer to the game than the game itself.

While OpenKneeboard is not bug-free, *every* time I have investigated an ordering requirement issue, it has turned out to be bug in the other layer; while I no longer investigate unknown interactions between layers, I am happy to investigate issues if there is evidence that OpenKneeboard has a bug, including dependencies on unspecified behavior.

### What requirements does OpenKneeboard have for spaces and poses?

- Spaces and poses must be handled consistently; see [below](#what-do-you-mean-by-consistent-handling) for details.
- It currently uses `VIEW` and `LOCAL` spaces; it is likely to use `STAGE` and `LOCAL_FLOOR` in the future. They are currently avoided as limitations of previous generations of headsets made them less useful.
- Your layer must handle multiple `xrSpace` handles referring to the same reference space, as OpenKneeboard calls `xrCreateReferenceSpace()` independently of the game, instead of attempting to track and re-use the game's `xrSpace`s. The reasons for this are:
  - It is permitted, and would also be valid for the game to create multiple spaces referring to the same reference space
  - Attempting to re-use would greatly increase complexity
  - It allows a consistent behavior regardless of which spaces the game uses. For example, some games only use `VIEW` and `LOCAL`, some use `STAGE`, some use both `LOCAL` and `STAGE`, and future games are likely to use `LOCAL_FLOOR` instead of the combination
- Your layer must consistently handle poses specified in `XrCompositionLayerQuad` structures in `xrEndFrame()`

In most frames, OpenKneeboard just re-submits its quad layers with the same space (currently derived from `LOCAL`) and pose as the previous frame; if you see movement and you have not recentered OpenKneeboard that frame, this indicates that your layer is not handling poses and spaces consistently.

When a recentering binding is pressed, it calls `xrLocateSpace()`, comparing `VIEW` to `LOCAL`, and stores the offset. This offset is then used for the new frame, and all future frames, until the next recentering.

The three most common causes of bad interactions between OpenKneeboard and other API layers are:

- the other API layer assumes only one space will be created for each reference space type
- the other API layer does not consistently handle all active spaces/poses
- the other API layer reads swapchain images, and assumes that the swapchain will be updated every frame; this is not required and OpenKneeboard does not do it by default. You can test if this is the cause of the problem by turning on 'update swapchains every frame' under "Compatibility Quirks" in OpenKneeboard's advanced setings

Several other API layers have had bugs due to tracking more information about spaces than they need to; you will usually need to keep track of the reference space type for each `xrSpace`, but not more than that. Calling `xrLocateSpace()` many times per frame should be efficient enough and is much less error-prone than tracking the games' calls.

### What do you mean by 'consistent handling'?

1. Within a single frame, xrSpaces and poses must be handled consistently. This includes both in the results of `xrLocateSpace()`, and in the handling of poses in submitted `XrCompositionLayerQuad` structures. For example:
    1. I ask "where is space *a* compared to space *b*?"; say the answer is "space *a* is 1 meter to the left of space *b*"
    2. I ask "where is space *c* compared to space *b*?"; say the answer is "space *c* is 1 meter to the right of space *b*"
    3. If I then ask "where is space *c* compared to space *a*?", the answer **must** be 1 + 1 = 2, i.e. "space *c* is 2 meters to the right of space *a*'.
    4. If I submit a quad layer with a position 0.5 meters to the right of space *b*, it should appear half way between spaces *b* and *c*
    5. If I submit a quad layer with a position 1.5 meters to the right of space *a*, it should appear in the same position as if it were positioned 0.5 meters to the right of space *b*
    6. Equivalent requirements apply for orientation
2. World-locked positions (i.e. based on local, stage, or local_floor spaces) should be consistent *between* frames, in terms of in-game world. If your layer manipulates poses, this implies that it probably will not be in terms of real-world space. For example, if I submit `(1, 1, -1) in myXrSpaceDerivedFromLocal` and it appears at a certain in-game position, *any* later frames that also use `(1, 1, -1) in myXrSpaceDerivedLocal` should be in the same in-game position

### How do I investigate what's going on?

[OpenXR-Tracing] is a useful tool for this, combined with [Tabnalysis](https://apps.microsoft.com/detail/9nqlk2m4rp4j).

You can also use [the OpenXR Conformance Test Suite](https://github.com/KhronosGroup/OpenXR-CTS/releases/latest); I recommend:

1. Disabling all API layers
2. Running the following tests and making sure they pass
3. Enabling your API layer and doing any required configuration
4. Running the tests again. For interactive tests, do any required recentering/calibration/... required for your layer to have an effect

Recommended tests:

- `conformance_cli.exe -G D3D11 --apiVersion 1.0 "QuadHands"` is often a clear example of problems; make sure that the quad layers move with the cubes while your layer is manipulating poses
- `conformance_cli.exe -G D3D11 --apiVersion 1.0 "XrCompositionLayerQuad"` is a non-interactive test checking technical correctness
- `conformance_cli.exe -G D3D11 --apiVersion 1.0 "[composition][interactive]"` includes QuadHands among a variety of other related tests

For interactive tests, you will be prompted to press 'select' or 'menu'; on an oculus touch controller via Link or Airlink, these are the 'A' and 'B' buttons, not the actual menu and select buttons. For other controllers, the binding will vary.

### I found a bug in OpenKneeboard's handling of spaces and poses

Great! Please open an issue on GitHub with details - ideally traces from [OpenXR-Tracing] - showing the incorrect behavior.

[OpenXR-Tracing]: https://github.com/fredemmott/OpenXR-Tracing