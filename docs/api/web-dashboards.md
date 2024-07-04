---
parent: APIs
---

# Web Dashboard APIs
{: .no_toc }

OpenKneeboard currently has a very limited JavaScript API for Web Dashboard tabs; this is likely to be extended in the future. Future plans are primarily being discussed in [issue #240](https://github.com/OpenKneeboard/OpenKneeboard/issues/240)

## Table of Contents
{: .no_toc, .text-delta }

1. TOC
{:toc}

## Detecting OpenKneeboard

There are several ways to detect if your web page is being accessed within OpenKneeboard's "Web Dashboard" tabs:

- the user agent string contains `OpenKneeboard/a.b.c.d`, where `a.b.c.d` is the version number
- `window.OpenKneeboard` is set, and is a valid object
- the `<body>` element has the `OpenKneeboard` class
- the `<body>` element has the `OpenKneeboard_WebView2` class; this also indicates that OpenKneeboard is using Microsoft's Chromium-based-Edge 'WebView2' browser engine

## JavaScript APIs

All APIs are methods on the `window.OpenKneeboard` object; it is best practice to confirm that OpenKneeboard is being used and the API you want is available; for example, if you want to call [`SetPreferredPixelSize()`](#openkneeboardsetpreferredpixelsize), a safe way to call it is like this:

```JavaScript
if (window.OpenKneeboard?.SetPreferredPixelSize) {
    OpenKneeboard.SetPreferredPixelSize(/* args go here */);
}
```

### `OpenKneeboard.SetPreferredPixelSize()`

Set the preferred pixel size and aspect ratio for this page.

```JavaScript
OpenKneeboard.SetPreferredPixelSize(width, height);
);
```

`width` and `height`:
- must be an integer number of pixels
- must be greater than or equal to 1
- must be less than or equal to `D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION` (16384 as of Direct3D 11.1).