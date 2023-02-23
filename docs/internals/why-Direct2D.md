---
title: Why Direct2D
parent: Internals
---

# Why does the app use Direct2D?

Initially, the app used wxDC, wxImage, and wxBitmap for all 2D drawing
operations; I replaced these with Direct2D:
- for full alpha channel support
- as a learning experience (honestly, I expected to revert my changes without committing)
- with the expectation that there would not be a significant performance difference

That expectation was wrong: it removed many multi-second freezes when handling
large amount of image data. The primary reason seems to be:
- wxBitmap supports native pixel formats, but supports very few operations.
- wxImage supports common operations (e.g. scaling), but uses RGB, with the alpha
  channel stored separately.
- so, every conversion between the two required iterating over every pixel, on the CPU.

Moving to Direct2D:
- adds full alpha channel support
- reduces the number of pixel format conversions
- moves most operations from the CPU to the GPU
