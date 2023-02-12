# Supported Messages

The same messages are supported for Lua and C, and are used by the remote-control utility executables.

Messages have a name and a value; the value is always a string. It is the responsibility of the caller to convert objects or arrays to JSON strings if needed.

To find IDs for the events that require them and for information on the tradeoffs of IDs vs names or indices, see the [remote controls documentation](../remote-controls.md).

## Notes for OpenKneeboard v1.3 and below

In OpenKneeboard v1.3 and below, all messages start with `com.fredemmott.openkneeboard/` - for example, `RemoteUserAction` is `com.fredemmott.openkneeboard/RemoteUserAction`. This extra prefix has been removed for v1.4.

# MultiEvent

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

# RemoteUserAction

This message tells OpenKneeboard to take an action with no parameters, as if the corresponding remote control executable or input binding was pressed.

The value is one of:

- `PREVIOUS_BOOKMARK`
- `NEXT_BOOKMARK`
- `TOGGLE_BOOKMARK`
- `PREVIOUS_TAB`
- `NEXT_TAB`
- `PREVIOUS_PAGE`
- `NEXT_PAGE`
- `PREVIOUS_PROFILE`
- `NEXT_PROFILE`
- `TOGGLE_VISIBILITY`
- `TOGGLE_FORCE_ZOOM`
- `SWITCH_KNEEBOARDS`
- `RECENTER_VR`
- `HIDE`
- `SHOW`
- `ENABLE_TINT`
- `DISABLE_TINT`
- `TOGGLE_TINT`
- `INCREASE_BRIGHTNESS`
- `DECREASE_BRIGHTNESS`

`TINT`, `BRIGHTNESS`, and `BOOKMARK` actions are new in v1.4 and above.

# SetTabByID

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

# SetTabByName

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

# SetTabByIndex

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

# SetProfileByID

Value: JSON-encoded object:

```json
{
	"ID": "PROFILE_ID_HERE"
}
```

- `ID`: *string* - the ID of the profile to switch to

# SetProfileByName

Value: JSON-encoded object:

```json
{
	"Name": "My Profile"
}
```

- `Name`: *string* - the name of the profile to switch to

# SetBrightness

**This message will be added in v1.4.**

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
