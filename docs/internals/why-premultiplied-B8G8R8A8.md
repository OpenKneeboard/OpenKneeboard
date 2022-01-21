# Why B8G8R8A8 with pre-multiplied alpha?

This isn't an intuitive format for humans, however practically every graphics
API supports this efficiently on all modern hardware. This includes:

- GDI
- Direct2D
- Direct3D 11
- Direct3D 12
- OpenGL
- Vulkan
- Windows Imaging Component (image file loading)

More intuitive formats such as R8G8B8A8 - or either ordering with straight
alpha - are not as widely supported, either at the API or hardware level.

By sticking with this format, we never need to explicitly convert formats, and
practically all operations (including scaling) can be performed on the GPU.

This is also why this pixel format is used for the pixel data in SHM: we aim to
do as little as possible in injected processes - as practically everything
supports this pixel data format, we do not need to do format conversions.

## What is premultipled alpha?

It's an optimization, letting you do some of the work in advance for common
operations - especially painting a translucent image/texture on top of
another image/texture.

For alpha, it's easiest to think of color components as numbers between
0.0 and 1.0, instead of 0x00 and 0xFF; for example, in RGBA order with straight
alpha, 75%-opaque red is (1.0, 0.0, 0.0, 0.75).

Say we want to paint that on top of a pixel that is currently color (r1, g1, b1).

The resulting red component is:

`(1.0 * 0.75) + (r1 * (1 - 0.75))`

which is:

`(0.75 + (r1 * 0.25)`

Premultipled alpha takes advantage of the fact that the left part of the 
expression is constant, regardless of the color we're painting on top of: it's
always `1.0 * 0.75 = 0.75`, so we can do that in advance.

So, (1.0, 0.0, 0.0, 0.75) with straight alpha is equivalent to
(0.75, 0.0, 0.0, 0.75) with premultiplied alpha - or
(0.5, 0.0, 0.0, 0.75) becomes (0.375, 0.0, 0.0, 0.75).

This leads to premultiplied alpha being a common format for images/textures at
runtime, but it's comparitively rare as a file format: as images generally use
integers (e.g. 0x00-0xFF) for pixel values instead of floats, premultiplying
alpha is a lossy operation - though the information being lost is generally
invisible.
