---
parent: APIs
---

# Supported Messages
{: .no_toc }

The same messages are supported for Lua and C, and are used by the remote-control utility executables.

Messages have a name and a value; the value is always a string. It is the responsibility of the caller to convert objects or arrays to JSON strings if needed.

To find IDs for the events that require them and for information on the tradeoffs of IDs vs names or indices, see the [remote controls documentation](../features/remote-controls.md).

## Table of Contents
{: .no_toc .text-delta}

1. TOC
{:toc}

## MultiEvent

Used to send multiple events in one go for performance; this can sometimes increase performance compared to sending separate messages.

Value: a JSON-encoded array; each element is a two-element array containing another message name and string value - **not** nested JSON. For Example:

```
"[
	[
		\"SetProfileByName\",
		"{\"Name\": \"Some Profile\"}" ],
	[
		\"SetTabByName\",
		"{\"Name\": \"Some Tab\", \"Page Number\": 1, \"Kneeboard\": 1 }"
	]
]"
```

## RemoteUserAction

This message tells OpenKneeboard to take an action with no parameters, as if the corresponding remote control executable or input binding was pressed.

The value is one of:

- `PREVIOUS_BOOKMARK`
- `NEXT_BOOKMARK`
- `TOGGLE_BOOKMARK`
- `PREVIOUS_TAB`
- `NEXT_TAB`
- `PREVIOUS_PAGE`
- `NEXT_PAGE`
- `PREVIOUS_PROFILE` - *see profiles warning below*
- `NEXT_PROFILE`: - *see profiles warning below*
- `TOGGLE_VISIBILITY`
- `TOGGLE_FORCE_ZOOM`
- `SWAP_FIRST_TWO_VIEWS` - *added in v1.7; replaced `SWITCH_KNEEBOARDS` in earlier versions*
- `RECENTER_VR`
- `HIDE`
- `SHOW`
- `ENABLE_TINT`
- `DISABLE_TINT`
- `TOGGLE_TINT`
- `INCREASE_BRIGHTNESS`
- `DECREASE_BRIGHTNESS`


**WARNING**: like changing profiles in the app, or via a remote control, changing profiles via the API will discard all of the user's notes, bookmarks, and all other state, e.g. the current page for each tab, DCS radio history, etc.

## SetTabByID

Value: JSON-encoded object:

```json
{
	"ID": "TAB_ID_GOES_HERE",
	"PageNumber": 0,
	"Kneeboard": 0,
}
```

- `ID`: *string* - the ID of the tab
- `PageNumber`: *optional integer* - the page number to switch to within the tab; `0` (or omit) to return to the same page that the tab was last open on
- `Kneeboard`: *optional integer* - `1` to switch to the primary kneeboard (usually on the right), `2` to the secondary (usually on the left); `0` (or omit) to switch the tab for the current kneeboard

## SetTabByName

Value: JSON-encoded object:

```json
{
	"Name": "My Tab Title",
	"PageNumber": 0,
	"Kneeboard": 0,
}
```

- `Name`: *string* - the title of the tab to switch to
- `PageNumber`, `Kneeboard`: *optional integer* - see [SetTabByID](#settabbyid).

## SetTabByIndex

Value: JSON-encoded object:

```json
{
	"Index": 0,
	"PageNumber": 0,
	"Kneeboard": 0,
}
```

- `Index`: *integer* - the position of the tab to switch to; tab indices start at 0, not 1.
- `PageNumber`, `Kneeboard`: *optional integer* - see [SetTabByID](#settabbyid).

## SetProfileByID

Value: JSON-encoded object:

```json
{
	"ID": "PROFILE_ID_HERE"
}
```

- `ID`: *string* - the ID of the profile to switch to

**WARNING**: like changing profiles in the app, or via a remote control, changing profiles via the API will discard all of the user's notes, bookmarks, and all other state, e.g. the current page for each tab, DCS radio history, etc.

## SetProfileByName

Value: JSON-encoded object:

```json
{
	"Name": "My Profile"
}
```

- `Name`: *string* - the name of the profile to switch to

**WARNING**: like changing profiles in the app, or via a remote control, changing profiles via the API will discard all of the user's notes, bookmarks, and all other state, e.g. the current page for each tab, DCS radio history, etc.

## SetProfileByName

## SetBrightness

Value: JSON-encoded Object:

```json
{
	"Mode": "Absolute",
	"Brightness": 1.0
}
```

- `Brightness`: the desired brightness
- `Mode`: *optional string* - must be `Absolute` or `Relative`. Defaults to `Absolute` if not provided.
   - `Absolute`: Tint and brightness is enabled, and brightness is set to `Brightness`, which must be between `0.0` (minimum brightness) and `1.0` (maximum brightness)
   - `Relative`: Tint and brightness is enabled, and `Brightness` is added to the current kneeboard brightness. `Brightness` must be between `-1.0` and `1.0`

`Brightness` must be a float, not an integer - for example, `0` and `1` are not valid values, and must be replaced with `0.0` or `1.0`.

## SetVRView

Changes a kneeboard's VR settings, as if the user changed them in OpenKneeboard's settings; the change is saved to the current profile.

Value: JSON-encoded Object:

```json
{
	"Mode": "Relative",
	"Kneeboard": 0,
	"VerticalDistance": -0.01,
	"Yaw": 5.0
}
```

- `Mode`: *optional string* - must be `Absolute` or `Relative`. Defaults to `Absolute` if not provided.
  - `Absolute`: each provided value replaces the current value, and must be within the range listed below
  - `Relative`: each provided value is added to the current value; the result is limited to the range listed below, and rotations wrap around
- `Kneeboard`: *optional integer* - `1` for the primary kneeboard, `2` for the secondary; `0` (or omit) for the active kneeboard
- all other values are *optional*; values that are not provided are not changed. Units, signs, and ranges match the settings app:
  - `MaxWidth`, `MaxHeight`: *float* - kneeboard width and height limits, in meters; at least `0.01`
  - `VerticalDistance`: *float* - vertical distance from eye level, in meters; positive values are below eye level
  - `HorizontalPosition`: *float* - left-to-right position, in meters
  - `ForwardPosition`: *float* - forward position, in meters
  - `Pitch`, `Roll`, `Yaw`: *float* - in degrees, from `-180.0` to `180.0`
  - `GazeTargetHorizontalScale`, `GazeTargetVerticalScale`: *float* - gaze target size, from `0.0` to `4.0`
  - `ZoomScale`: *float* - zoom level when looking at the kneeboard, from `1.0` to `4.0`
  - `NormalOpacity`: *float* - opacity when not looking at the kneeboard, from `0.0` to `1.0`
  - `GazeOpacity`: *float* - opacity when looking at the kneeboard, from `0.0` to `1.0`
  - `EnableGazeZoom`: *boolean* - zoom when looking at the kneeboard; not affected by `Mode`
  - `DisplayArea`: *string* - `Full`, or `ContentOnly` to hide the header and footer; not affected by `Mode`

If any provided value is invalid, the event is ignored and no settings are changed. Kneeboards that mirror another kneeboard do not have their own VR settings, and can not be changed with this event.

As with `SetBrightness`, float values must be written as floats - for example, `5.0`, not `5`.

## Requesting additional APIs

Keep in mind the purpose of OpenKneeboard: OpenKneeboard is a tool for users to show their content how they wish in VR, via OpenKneeboard's settings. It is not a developer toolkit.

If your request fits that purpose, search for an existing feature request GitHub issue. If there is one, add a thumbs up reaction to the top post, otherwise create a new one. Do not add '+1'/'me too' comments.
