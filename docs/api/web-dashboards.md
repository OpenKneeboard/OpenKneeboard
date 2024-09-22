---
title: Web Dashboards
parent: APIs
has_children: true
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
- *DEPRECATED - removed in v1.9*: the `<body>` element has the `OpenKneeboard` class
- *DEPRECATED - removed in v1.9*: the `<body>` element has the `OpenKneeboard_WebView2` class; this also indicates that OpenKneeboard is using Microsoft's Chromium-based-Edge 'WebView2' browser engine

OpenKneeboard v1.9 and above no longer add CSS classes to the body, as this can lead to compatibility problems. The easiest way to restore the old behavior is to add the following to your page:

```js
if (windows.OpenKneeboard) {
  document.body.classList.add("OpenKneeboard");
}
```

The `window.OpenKneeboard` object is loaded before any scripts on the page start executing.

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

In v1.9 and above, `window.OpenKneeboard` can usually be accessed just as `OpenKneeboard`.

### Conventions in v1.9+

The JS API is extended and improved in v1.9 and above:

Functions have consistent return behavior:
- they return a `Promise`, which can be used with `await`, `promise.then()`, or other standard techniques
- if the success value is undocumented or documented as `any`, it may contain additional information intended to inform you, but may change without warning between versions. Do not write code that depends on a particular value or type
- on failure:
  - the error will be logged to the developer tools console
  - the `Promise` will be rejected with an `OpenKneeboardAPIError`

The `OpenKneeboardAPIError` extends the standard JavaScript `Error` class, adding an `apiMethodName` string property; for example:

```js
// In v1.9+:
try {
  /* ... some code here ... */
  await OpenKneeboard.SomeAPI(/* ... */ );
  /* ... some more code ... */
} catch (error) {
  if (error instanceof OpenKneeboardAPIError) {
    // Logs "OpenKneeboard.SomeAPI()" and an explanation
    console.log(error.apiMethodName, error.message);
  }
}
```

### A note on `await`

Several OpenKneeboard API functions return a `Promise`; the examples will use the `await` syntac for these, e.g.:

```js
try {
  await OpenKneeboard.DoSomething();
  DoSomethingElse();
} catch (error) {
  HandleError(error);
}
```

As these are standard `Promise` objects, if you prefer, you can use the traditional syntax instead:

```js
OpenKneeboard.DoSomething()
  .then(function(result) { DoSomethingElse(); })
  .catch(function(error) {HandleError(error); });
```

### TypeScript

In v1.9 and above, TypeScript definitions are included both in `data` subfolder of the OpenKneeboard source tree, and in the `share\doc` subfolder of an OpenKneeboard installation.

Unless you are intentionally using an experimental API, you should only include `OpenKneeboard.d.ts` in your project; the various `OpenKneeboard-Experimental-*.d.ts` files define additional features that are not usually available.

If you are not familiar with TypeScript, you may still want to copy the `OpenKneeboard.d.ts` file into your project - some IDEs will automatically use it to provide better help while you are writing your JavaScript code.

All of OpenKneeboard's stable APIs are defined in [`OpenKneeboard.d.ts`](https://github.com/OpenKneeboard/OpenKneeboard/blob/master/data/OpenKneeboard.d.ts).

### SetPreferredPixelSize

Set the preferred pixel size and aspect ratio for this page.

```js
// Syntax (v1.8):
window.OpenKneeboard.SetPreferredPixelSize(width: number, height: number): undefined;
// Example (v1.8):
window.OpenKneeboard.SetPreferredPixelSize(1024, 768);

// Syntax (v1.9+):
OpenKneeboard.SetPreferredPixelSize(width: number, height: number): Promise<any>;
// Example (v1.9+):
await OpenKneeboard.SetPreferredPixelSize(1024, 768);
```

`width` and `height`:
- must be an integer number of pixels
- must be greater than or equal to 1
- must be less than or equal to `D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION` (16384 as of Direct3D 11.1).

In v1.9+, awaiting/resolving the promise is *optional*, but does provide a means to detect success or failure; for compatibility, it can also be accessed via the `window.OpenKneeboard`

### GetVersion

*This requires OpenKneeboard v1.9 and above.*

```js
// Syntax:
OpenKneeboard.GetVersion(): Promise<Object>;

// Example:
const okbVersion = await OpenKneeboard.GetVersion();
```

The promise resolves to an object like the following:

```json
{
  "Components": {
    "Major": 1,
    "Minor": 9,
    "Patch": 0,
    "Build": 0
  },
  "HumanReadable": "v1.9.0+local",
  "IsGitHubActionsBuild": false,
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

## OpenDeveloperToolsWindow

Opens the MS Edge developer tools window; requires OpenKneeboard v1.9.

```js
OpenKneeboard.OpenDeveloperToolsWindow();
```

## Experimental JavaScript APIs

These features will be changed without notice, and in any update, including patch releases; you should avoid using them in released software if possible; reach out in `#code-talk` [on Discord](https://go.openkneeboard.com/discord) before releasing software that uses them.

If you want to use an experimental API with the goal of switching to a stable API when it is used, the best approach is to wrap enabling it in a `try`/`catch`, e.g.:

```js
try {
  await OpenKneeboard.EnableExperimentalFeature('foo', 1234);
  TurnOnMyUseOfThisExperimentalFeature();
} catch (ex) {
  console.log("Failed to initialize experimental feature foo", ex);
}
```

Experimental features are identified by a name, and a version number (usually `YYYYMMDDnn`, where 'nn' increments if there are multiple revisions within a day); they must be explicitly enabled.

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
- experimental feature: `RawCursorEvents` version `2024071802`
- experimental feature: `SetCursorEventsMode` version `2024071801`

This event provides access to OpenKneeboard's raw cursor events, bypassing browser mouse emulation.

```js
// Example:
OpenKneeboard.addEventListener('cursor', (ev) => { console.log(ev.detail); });
await OpenKneeboard.EnableExperimentalFeatures([
  {name: "RawCursorEvents", version: 2024071802},
  {name: "SetCursorEventsMode", version: 2024071801},
]);
await OpenKneeboard.SetCursorEventsMode("Raw");
```

For version 2024071802 of the experimental feature, the events will be of this form:

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

An example/debugging tool - `api-example-RawCursorEvents.html` is included in the `data` folder of the OpenKneeboard source tree, or the `share\doc\` subfolder of an OpenKneeboard installation.

#### Change history

##### 2024071801 -> 2024071802

- enabling the feature now only makes the cursor event mode available, it does not automatically switch to it
- it is no longer incompatible with the `DoodlesOnly` feature

### DoodlesOnly

This feature requires:
- OpenKneeboard v1.9 or above
- experimental feature: `DoodlesOnly` version `2024071802`

Enabling this feature disables the standard mouse emulation, and instead uses OpenKneeboard's standard "draw on top" behavior, letting users take notes or sketch on top of your content.

As the mouse emulation is disabled, after enabling this feature, it is no longer possible for the user to interact with your web page, including the scroll bars; it should only be used for single-screen content.

```js
// Example:
await OpenKneeboard.EnableExperimentalFeatures([
  {name: "DoodlesOnly", version: 2024071802},
  {name: "SetCursorEventsMode", version: 2024071801},
]);
await OpenKneeboard.SetCursorEventsMode("DoodlesOnly");
```

#### Change history

##### 2024071801 -> 2024071802

- enabling the feature now only makes the cursor event mode available, it does not automatically switch to it
- it is no longer incompatible with the `RawCursorEvents` feature

### SetCursorEventsMode

This feature requires:
- OpenKneeboard v1.9 or above
- experimental feature: `SetCursorEventsMode` version `2024071801`

```js
// Syntax:
OpenKneeboard.SetCursorEventsMode("MouseEmulation" | "DoodlesOnly" | "Raw"): Promise<any>;
// Example:
await OpenKneeboard.EnableExperimentalFeatures([
  {name: "RawCursorEvents", version: 2024071802},
  {name: "SetCursorEventsMode", version: 2024071801},
]);
await OpenKneeboard.SetCursorEventsMode("Raw");
```

- setting the mode to `"Raw"` requires `RawCursorEvents` version `2024071802`
- setting the mode to `"DoodlesOnly"` requires `DoodlesOnly` version `2024071802`

An example/debugging tool - `api-example-RawCursorEvents.html` is included in:
- [the `data` folder of the OpenKneeboard source tree](https://github.com/OpenKneeboard/OpenKneeboard/blob/master/data/api-example-RawCursorEvents.html)
- the `share\doc\` subfolder of an OpenKneeboard installation.

### Page-Based Web Applications

This allows creating a web page that works like a native OpenKneeboard tab type with multiple pages, instead of a potentially-scrollable web view.

This functionality is documented [on a separate page](web-dashboards/page-based-content.md).