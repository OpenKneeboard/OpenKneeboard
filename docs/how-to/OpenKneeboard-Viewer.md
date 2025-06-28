---
parent: How-To
---

# OpenKneeboard-Viewer

`OpenKneeboard-Viewer.exe` is a program that shows the images that OpenKneeboard sends to other programs, such as games. It is primarily intended for debugging, but can also be useful for capturing a 2D version of the kneeboard for streaming.

It can be found in the `utilities` subfolder of OpenKneeboard's installation folder - usually `C:\Program Files\OpenKneeboard\utilities`.

## Keyboard Shortcuts

* `B`: **B**orderless mode - no window border or title bar
* `C`: **C**apture screenshot
* `I`: show **i**nformation, such as frame counter and layer (view)
* `1`-`9`: show the nth layer (view).
* `S`: Streamer Mode
* `F`: Switch background **F**ill: transparent, checkerboard, Windows default.
* `V`: Toggle between mirroring **V**R and non-VR content. This *only* affects the viewer, not the game or the rest of OpenKneeboard.
* `U`: Unscaled

## Graphics API

`OpenKneeboard-Viewer` can be launched with `-G D3D11`, `-G D3D12`, or `-G Vulkan`; this is a developer tool for debugging. It does not need to match the game.

## Streamer Mode

This makes the background transparent, and
disables the error message that is usually shown when
OpenKneeboard isn't running or the kneeboard is hidden.

Recommended usage with OBS:

1. Add a 'Window Capture' source
2. Set 'Capture Method' to 'Windows 10 (1903 and up)'
3. Tick the 'Client Area' box
4. Add a 'Color Key' filter with color set to magenta, and similarity and smoothness both set to 1
5. Right click on the source and select 'Scale Filtering' -> 'Point'

If you use two kneeboards, you will want to run and capture
two copies of the viewer; in the second one, press the `2` key
to switch it to showing the other kneeboard.
