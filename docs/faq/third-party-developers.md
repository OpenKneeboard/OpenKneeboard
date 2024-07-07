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

You have three options:
- implement a custom tab type in OpenKneeboard with C++ and Direct3D 11
- provide a file or web page, and instruct users on how to configure OpenKneeboard to access that content. OpenKneeboard will automatically reload files when they are modified.
- provide a window, and instruct users on how to configure a Window Capture tab for your app

OpenKneeboard has additional integration APIs available for [web dashboards](api/web-dashboards.md); however, text quality will be higher for text or PDF files.

Do not write software that changes OpenKneeboard's configuration files; it is *extremely* likely to break users' configuration when they update OpenKneeboard.

OpenKneeboard is a tool for users to show their content how they wish in VR, via OpenKneeboard's settings. It is not a developer toolkit.

Feel free to *read* OpenKneeboard's configuration files from your software, but keep in mind there are no attempts at providing stability.

## How do I choose where my app's content is displayed in VR?

Instruct users on how to change their user settings to your recommendations.

OpenKneeboard is a tool for users to show their content how they wish in VR, via OpenKneeboard's settings. It is not a developer toolkit.

Do not write software that changes OpenKneeboard's configuration files; it is *extremely* likely to break users' configuration when they update OpenKneeboard.

## How do I use OpenKneeboard to create my own OpenXR overlay?

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

## API layers that manipulate poses

### What should the API layer order be?

Pose manipulation layers should be closer to the runtime and farther from the game than OpenKneeboard; this is because **OpenKneeboard does nothing that the game itself is not allowed to do**, and pose manipulation layers generally want to manipulate OpenKneeboard's world-locked content the same way as they manipulate the game world, along with any world-locked content the game produces (e.g. HUDs and menus are world-locked overlays in some games).

If OpenKneeboard is closer to the runtime than your layer, OpenKneeboard's world-locked content should be locked to the real-world instead of your modified world space, which is generally undesirable, but otherwise everything should work fine.

If your pose manipulation layer needs to be between the game and OpenKneeboard, your layer has incorrect behavior, and will also be broken for some games: OpenKneeboard does nothing that the game itself is not allowed to do, some games to do the same things, and it is not possible for your layer to be closer to the game than the game itself. 

### What do you mean by 'consistent handling'?

1. Within a single frame, xrSpaces and poses must be handled consistently. This includes both in the results of `xrLocateSpace()`, and in the handling of poses in submitted `XrCompositionLayerQuad` structures. For example:
    1. I ask "where is space *a* compared to space *b*?"; say the answer is "space *a* is 1 meter to the left of space *b*"
    2. I ask "where is space *c* compared to space *b*?"; say the answer is "space *c* is 1 meter to the right of space *b*"
    3. If I then ask "where is space *c* compared to space *a*?", the answer **must** be 1 + 1 = 2, i.e. "space *c* is 2 meters to the right of space *a*'.
    4. If I submit a quad layer with a position 0.5 meters to the right of space *b*, it should appear half way between spaces *b* and *c*
    5. If I submit a quad layer with a position 1.5 meters to the right of space *a*, it should appear in the same positon as if it were positioned 0.5 meters to the right of space *b*
    6. Equivalent requirements apply for orientation
2. World-locked positions (i.e. based on local, stage, or local_floor spaces) should be consistent *between* frames, in terms of in-game world. If your layer manipulates poses, this implies that it probably will not be in terms of real-world space. For example, if I submit `(1, 1, -1) in myXrSpaceDerivedFromLocal` and it appears at a certain in-game position, *any* later frames that also use `(1, 1, -1) in myXrSpaceDerivedLocal` should be in the same in-game position

### What requirements does OpenKneeboard have for spaces and poses?

- Spaces and poses must be handled consistently as above
- It currently uses `VIEW` and `LOCAL` spaces; it is likely to use `STAGE` and `LOCAL_FLOOR` in the future. They are currently avoided as limitations of previous generations of headsets made them less useful.
- Your layer must handle multiple `xrSpace` handles referring to the same reference space, as OpenKneeboard calls `xrCreateReferenceSpace()` independently of the game, instead of attempting to track and re-use the game's `xrSpace`s. The reasons for this are:
  - It is permitted, and would also be valid for the game to create multiple spaces referring to the same reference space
  - Attempting to re-use would greatly increase complexity
  - It allows a consistent behavior regardless of which spaces the game uses. For example, some games only use `VIEW` and `LOCAL`, some use `STAGE`, some use both `LOCAL` and `STAGE`, and future games are likely to use `LOCAL_FLOOR` instead of the combination
- Your layer must consistently handle poses specified in `XrCompositionLayerQuad` structures in `xrEndFrame()`

  In most frames, OpenKneeboard just re-submits its' quad layers with the same space (currently derived from `LOCAL`) and pose as the previous frame; if you see movement and you have not recentered OpenKneeboard that frame, this indicates that your layer is not handling poses and spaces consistently.

  When a recentering binding is pressed, it calls `xrLocateSpace()`, comparing `VIEW` to `LOCAL`, and stores the offset. This offset is then used for the new frame, and all future frames, until the next recentering.

  The three most common causes of bad interactions between OpenKneeboard and other API layers are:
  - the other API layer assumes only one space will be created for each reference space type
  - the other API layer does not consistently handle all active spaces/poses
  - the other API layer reads swapchain images, and assumes that the swapchain will be updated every frame; this is not required and OpenKneeboard does not do it by default. You can test if this is the cause of the problem by turning on 'update swapchains every frame' under "Compatibility Quirks" in OpenKneeboard's advanced setings

  ### How do I investigate what's going on?

  [OpenXR-Tracing] is a useful tool for this, combined with [Tabnalysis](https://apps.microsoft.com/detail/9nqlk2m4rp4j).

  ### I found a bug in OpenKneeboard's handling of spaces and poses

  Great! Please open an issue on GitHub with details - ideally traces from [OpenXR-Tracing] - showing the incorrect behavior.

  [OpenXR-Tracing]: https://github.com/fredemmott/OpenXR-Tracing