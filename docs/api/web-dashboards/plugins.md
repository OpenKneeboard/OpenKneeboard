---
parent: Web Dashboards
---

# Plugins
{: .no_toc }

**THIS IS AN EXPERIMENTAL FEATURE** and may change between any two versions without notice; feedback and questions are very welcome in `#code-talk` [on Discord](https://go.openkneeboard.com/Discord).

Unlike most experimental APIs, it does not need to be explicitly enabled - creating or registering a plugin is sufficient opt-in.


## Table of Contents
{: .no_toc, .text-delta }

1. TOC
{:toc}

## Overview

A "plugin" is primarily a JSON file that defines additional tab types; these custom tab types have access to some additional JS APIs. Plugins require OpenKneeboard v1.9 or above

The JSON file can either be:

- located via the Windows registry. This is useful for third-party programs that involve locally running software
- distributed in a `.OpenKneeboardPlugin` file - this is just a zip file, which must contain a `v1.json`. This is useful for tools that do not require local installation, e.g. web sites.

See [Distribution](#distribution) for details.

Plugins will typically also use the [Page-Based Web Applications](./page-based-content.md) experimental feature.

## JSON File Format

```ts
{
    "ID": string,
    "Metadata": {
        "PluginName": string,
        "PluginReadableVersion": string,
        "PluginSemanticVersion": string,
        "OKBMinimumVersion": string,
        "OKBMaximumTestedVersion" ?: string,
        "Author" ?: string,
        "Website" ?: string,
    },
    "TabTypes": [TabType]
}
```

### The ID field

The 'ID' field is an arbitrary string of your choice which has negligible chances of conflicting with any other future plugin. Good IDs include: 

- A domain or subdomain you own; for example, if you owned `example.com`, you could use `someplugin.example.com`
- A project page for the plugin , e.g. `github.com/youruser/yourPlugin`
- A random UUID or GUID generated once, and only once, by you, then permanently embedded in the file before distribution

The ID field should not change between versions of the plugin, but if you publish multiple plugins, each plugin should have its' own ID.

If you dynamically generate plugins, the ID should be re-used if the user downloads the same plugin again later. A possible ID scheme here would be `myplugin.example.com/~USER/KNEEBOARD_ID`, where 'USER' is a user on your system (e.g. a content creator, or someone that can configure their own kneeboards/dashboards on your site), not necessarily a unique OpenKneeboard user.

Forbidden IDs include:

- anything literally using `example.com`, `youruser`, `yourplugin`, `yourdomain` etc
- anything referencing `openkneeboard.com`, `fredemmott.com`, `github.com/fredemmott`, `github.com/openkneeboard/` or anything similar
- UUIDs or GUIDs that are generated on installation, or when writing the JSON file

### Version fields

The `PluginReadableVersion` is an arbitrary string, which will only be used for displaying to the user.

All other version fields are interpreted as SEMVER.

### OpenKneeboard versions

OpenKneeboard versions can be specified with between 1 and 4 components, although just specifying '1' (equivalent to `1.0.0`) is probably a bad idea. I recommend using two components for the minimum version, and the full version for maximum tested version.

As the version field is SEMVER, you **must not** use the 4-dotted-integers form; for example, use `1.2.3+GHA.4` instead of `1.2.3.4`. `1.2.3.4` is not valid in SEMVER.

As only the latest version of OpenKneeboard version is supported, there is no need to truly determine the minimum version of OpenKneeboard that your plugin is compatible with. For `OKBMinimumVersion` I recommend:

- generally, use the latest version of OpenKneeboard
- if the latest version is not yet in auto-update, or has been out for less than 2 weeks, consider also supporting the previous version

For `OKBMaximumTestedVersion`, use the version number for the very latest version of OpenKneeboard you have tested with; do not attempt to guess future compatibility. OpenKneeboard will handle any known compatibility issues.

### Tab types

A `TabType` is a JSON object of this form:
```ts
{
    "ID": string,
    "Name": string,
    "Glyph" ?: string,
    "Implementation": "WebBrowser",
    "ImplementationArgs": {
        "URI": string,
        "InitialSize" ?: { "Width": number, "Height": number },
    },
    "CustomActions" ?: [CustomAction],
}
```

- `ID` **MUST** start with the plugin ID followed by a semicolon
- `ID` **MUST** be unique for each tab type
- `Name` is a human-readable name with no uniqueness constraints
- `Glyph` is optional; if present, it is a string containing a single unicode character from [the Segoe MDL2 Asset icons](https://learn.microsoft.com/en-us/windows/apps/design/style/segoe-ui-symbol-font)
- `URI` can be any standard URI (e.g. `file://`, `https://example.com`, `http://localhost:1234`), or - in v1.9.9 and above - `plugin://foo.html` to reference an HTML file inside the plugin zip.

`plugin://` URLs will be served from an `https://VARIES.openkneeboardplugins.localhost/` virtual host; no webserver is created, but your JavaScript will believe the domain exists and that HTTPS is being used.

### Custom actions

A `CustomAction` is a JSON object of this form:

```ts
{
    "ID": string,
    "Name": string,
}
```

- `ID` **MUST** start with the tab type ID followed by a semicolon
- `ID` **MUST** be unique for each custom action

In future versions of OpenKneeboard, custom actions *may* be bindable via the settings UI; for now, they can be invoked via [exe files, or OpenKneeboard's C and LUA APIs](#invoking-custom-actions). The JSON format and API is designed to make this possible without needing any changes to plugins.

## Invoking custom actions

A future version of OpenKneeboard *may* add direct support for binding custom actions via the settings UI; for now, there are two approaches:

- executing a helper program similar to [the remote controls](../../features/remote-controls.md); this can be used with a StreamDeck, VoiceAttack, joy2key, or similar
- OpenKneeboard's C and LUA APIs

### The executable

OpenKneeboard's installation folder contains `OpenKneeboard-Plugin-Tab-Action.exe`

- if you tell your users to manually configure a tool to run this, you probably want to point them to `C:\Program Files\OpenKneeboard\utilities`
- if you are invoking custom actions from your own software:
  - it is *strongly* recommended to use the C API instead
  - if that is not possible, retrieve the path from the Windows registry, in `HKEY_CURRENT_USER\Software\Fred Emmott\OpenKneeboard\InstallationUtilitiesPath`

Usage:

```
OpenKneeboard-Plugin-Tab-Action.exe CUSTOM_ACTION_ID [EXTRA_JSON_DATA]
```

`EXTRA_DATA` is a string containing JSON.

For example:

```
OpenKneeboard-Plugin-Tab-Action "myplugin.example.com;mytab;myaction"
```

or, with data:

```
PS> OpenKneeboard-Plugin-Tab-Action `
  "myplugin.example.com;mytab;myaction" `
  "{\"hello\": \"world\"}"
```

### The C and Lua APIs

An additional message type is available, and used in the same way as other messages [in Lua](../lua.md), and [in C and other languages](../c.md).

- The API message type is `"Plugin/Tab/CustomAction"`
- The value is a JSON string of this form:

```ts
{
    "ActionID": string,
    "ExtraData"?: string,
}
```

- ActionID must exactly match a defined custom action ID
- ExtraData is an optional string that must itself contain JSON in strong form - it **MUST NOT** directly contain nested JSON data.

### Handling custom actions

Listen to the `plugin/tab/customAction` event; the event object is a `CustomEvent`, whose detail contains:
- `id`: the custom action ID
- `extraData`: optional; any caller-supplied data, decoded as JSON

## Distribution

### OpenKneeboardPlugin files

To create a user-installable `.OpenKneeboardPlugin` file:

- name your JSON file `v1.json`
- Create a zip containing that file
- Change the extension from `.zip` to `.OpenKneeboardPlugin`

Plugins are zipped to discourage browsers from showing the JSON text when plugins are made available for download.

For example, to package a JSON file as a plugin with PowerShell:

```powershell
$Out = "Example.OpenKneeboardPlugin";
$Zip = "$Out.zip"
Remove-Item $Out
if (Test-Path $Zip) {
    Remove-Item $Zip
}
Compress-Archive v1.json -DestinationPath $Zip
Rename-Item $Zip $Out
```

### Windows registry

If your software has an installer or other locally running code, you can directly register your plugin by creating a DWORD value in `SOFTWARE\Fred Emmott\OpenKneeboard\Plugins\v1`; both `HKEY_LOCAL_MACHINE` and `HKEY_CURRENT_USER` are supported.

- the value name should be the full path to your JSON file
- the value itself should be 0 for disabled, 1 for enabled
- the JSON file may have any name

You **MUST NOT** create your JSON file in `C:\Program Files\OpenKneeboard`, `%LOCALAPPDATA%\OpenKneeboard`, `Saved Games\OpenKneeboard`, or any other location that can reasonably be considered to belong to OpenKneeboard.

Good locations include *your application*'s LocalAppData folder.
