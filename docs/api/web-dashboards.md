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
    window.OpenKneeboard.SetPreferredPixelSize(width, height);
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

In version 1.9 and above, this will return a `Promise`; if it resolves, it will indicate the actual size used. If the operation fails, it will be rejected with a string indicating the error.

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

## Experimental JavaScript APIs

These features will be changed without notice; you should avoid using them in released software if possible; reach out in `#code-talk` [on Discord](https://go.openkneeboard.com/discord) before releasing software that uses them.

Experimental features are identified by a name, and a version number (usually `YYYYMMDDnn`, where 'nn' increments if there are multiple revisions within a day); they must be explicitly enabled.

### GetAvailableExperimentalFeatures

This function will fail with an exception in tagged releases of OpenKneeboard; it is *only* an interactive introspection tool to aid development. This functions requires OpenKneeboard v1.9 or above.

It will return a `Promise`; example usage:

```js
await OpenKneeboard.GetAvailableExperimentalFeatures()
```

The resulting value will contain the names and versions of all available experimental features, but the format is intentionally undefined, as it is *only* intended for interactive use, e.g. in a developer tools window.

### EnableExperimentalFeature

```js
// Syntax:
OpenKneeboard.EnableExperimentalFeature(name: string, version: number): Promise<any>;
// Example:
await OpenKneeboard.EnableExperimentalFeature("foo", 2024071801);
```

On success, the Promise will resolve to a JSON object, which may contain additional information for debugging.

On failure, the Promise will be rejected, with a string error message indicating the reason. Likely reasons for failure are unrecognized features or versions.

### EnableExperimentalFeatures

Enable multiple features in one go.

```js
// Example:
await OpenKneeboard.EnableExperimentalFeature([
  { name: "Foo", version: 1234 },
  { name: "Bar", version: 5678 },
]);
```

Return values will be the same; if any feature names/versions are not supported, the function will indicate failure - however, preceding features may have been enabled

### 'cursor' event

This feature requires:
- OpenKneeboard v1.9 or above
- experimental feature: `RawCursorEvents` version `2024071801`

This feature is incompatible with the 'DoodlesOnly' experimental feature.

This event provides access to OpenKneeboard's raw cursor events, bypassing browser mouse emulation.

Enabling this experimental feature will disable the mouse emulation, and start these events being emitted.

```js
// Example:
OpenKneeboard.addEventListener('cursor', (ev) => { console.log(ev.detail); });
await OpenKneeboard.EnableExperimentalFeature("RawCursorEvents", 2024071801);
```

For version 2024071801 of the experimental feature, the events will be of this form:

```js
{
  buttons: int,
  position: { x: number, y: number },
  touchState: "NearSurface" | "NotNearSurface" | "TouchingSurface",
}
```

These events are used for both tablet and mouse events.

- `buttons`: bitmask; the lowest bit is left click or the tablet pen tip. Other buttons are additional mice buttons or buttons on the tablet (not pen)
- `position`: positions in pixels
  - `x` and `y` are floating point; especially with a tablet, the cursor can be positioned between two logical pixels
- `touchState`
  - for tablets, this is self-descriptive
  - for mice, `NotNearSurface` means "outside of the web page's area", and `TouchingSurface` means 'clicking'
  - you should usually hide any cursors you may be showing if the state changes to `NotNearSurface`

 ### DoodlesOnly

This feature requires:
- OpenKneeboard v1.9 or above
- experimental feature: `DoodlesOnly` version `2024071801`

This feature is incompatible with the 'RawCursorEvents' experimental feature.

Enabling this feature disables the standard mouse emulation, and instead uses OpenKneeboard's standard "draw on top" behavior, letting users take notes or sketch on top of your content.

As the mouse emulation is disabled, after enabling this feature, it is no longer possible for the user to interact with your web page, including the scroll bars; it should only be used for single-screen content.

```js
// Example:
await OpenKneeboard.EnableExperimentalFeature("DoodlesOnly", 2024071801);
```
