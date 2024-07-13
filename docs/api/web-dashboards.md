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

## Transparency

OpenKneeboard's virtual browser window is fully transparent; this allows full RGBA transparency via standard CSS, e.g.:

```css
body.OpenKneeboard {
    background-color: transparent;
}
```

## JavaScript APIs

All APIs are methods on the `window.OpenKneeboard` object; it is best practice to confirm that OpenKneeboard is being used and the API you want is available; for example, if you want to call [`SetPreferredPixelSize()`](#setpreferredpixelsize), a safe way to call it is like this:

```js
if (window.OpenKneeboard?.SetPreferredPixelSize) {
    OpenKneeboard.SetPreferredPixelSize(width, height);
}
```

### SetPreferredPixelSize

Set the preferred pixel size and aspect ratio for this page.

```js
OpenKneeboard.SetPreferredPixelSize(width, height);
```

`width` and `height`:
- must be an integer number of pixels
- must be greater than or equal to 1
- must be less than or equal to `D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION` (16384 as of Direct3D 11.1).

### GetVersion

*This requires OpenKneeboard v1.9 and above.*

```js
const okbVersion = OpenKneeboard.GetVersion();
```

This function returns an object like the following:

```json
{
  "Components": {
    "Major": 1,
    "Minor": 9,
    "Patch": 0,
    "Build": 0
  },
  "HumanReadable": "v1.9.0+local",
  "IsGitHubActionBuild": false,
  "IsStableRelease": false,
  "IsTaggedVersion": false
}
```

- `Components`: individual parts of the OpenKneeboard as numbers; you probably want to use these for any version-dependent logic in your code. Released OpenKneeboard versions have versions of the form v*Major*.*Minor*.*Patch*.*Build* or v*Major*.*Minor*.*Patch*+GHA*Build*. `Build` is the run number from GitHub actions; it is 0 for local developer builds.
- `HumanReadable`: if you display an OpenKneeboard version number to the user in any form, you should use this string. For example, `v1.8.2-rc1` is much clearer to the user than `v1.8.2.2057` (the actual v1.8.2 release is `v1.8.2.2080`).
- `IsGitHubActionsBuild`: whether or not this version of OpenKneeboard was built automatically through continuous integration; otherwise, it is a local developer build
- `IsStableRelease`: true for builds made on GitHub Actions from a stable release tag; false for all other builds.
- `IsTaggedVersion`: true for builds made on GitHub Actions from a release tag, false for all other builds. This differs from `IsStableRelease` for tagged pre-release builds; for example, an automated build of `v1.9.0-rc1` would be a tagged version, but not a stable release.

The two forms of version number are used because:
- Some Windows features only support the 4-numbers form
- The string version is much more understandable to people than the 4-numbers form