# OpenKneeboard-Viewer.exe

`OpenKneeboard-Viewer.exe` is a program included in the 'remote control' zip shows the images that OpenKneeboard sends to other programs, such as games. It is primarily intended for debugging, but can also be useful for capturing a 2D version
of the kneeboard for streaming.

## Keyboard Shortcuts

* `I`: show information, such as frame counter and layer (kneeboard)
* `1`-`9`: show the nth layer (kneeboard). Only layers 1 and 2 are used by OpenKneeboard, for the primary and secondary kneeboard respectively
* `S`: Streamer Mode

## Streamer Mode

This switches the background color to solid magenta, and
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
