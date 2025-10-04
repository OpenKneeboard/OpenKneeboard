---
title: Remote Controls
parent: Features
---

# 'RemoteControl' exe Files

These are installed to `C:\Program Files\OpenKneeboard\utilities`, and can be used to control OpenKneeboard from anything that is able to launch other programs - for example:

- [Huion tablets](../how-to/huion.md)
- [an Elgato StreamDeck](./streamdeck.md)
- [VoiceAttack](./voice-attack.md)

## Third-Party Developers

It is *strongly* recommended you use [the API](../api/index.md) instead.

If you do write code that uses the remote controls:
- If you must support v1.8.8 and below, you should find them by following the [same process documented for the C API](../api/c.md#locating-the-dll); the remote controls are in `..\utilities\` relative to the DLL.
- If you only need to support v1.9 and above, find them using the `HKEY_CURRENT_USER\Software\Fred Emmott\OpenKneeboard\InstallationUtilitiesPath` registry value.

## Basic Remote Controls

Most remote controls are straightforward:

- `OpenKneeboard-RemoteControl-PREVIOUS_TAB.exe`: tell OpenKneeboard to go to the previous tab
- `OpenKneeboard-RemoteControl-NEXT_TAB.exe`: tell OpenKneeboard to go to the next tab
- `OpenKneeboard-RemoteControl-PREVIOUS_PAGE.exe`: tell OpenKneeboard to go to the previous page in the current tab
- `OpenKneeboard-RemoteControl-NEXT_PAGE.exe`: tell OpenKneeboard to go to the next page in the current tab
- `OpenKneeboard-RemoteControl-DECREASE_BRIGHTNESS.exe`: reduces the brightness
- `OpenKneeboard-RemoteControl-INCREASE_BRIGHTNESS.exe`: increases the brightness if previously reduced
- `OpenKneeboard-RemoteControl-ENABLE_TINT.exe`: enable the tint color set in advanced settings; this is usually used for a green tint for night vision
- `OpenKneeboard-RemoteControl-DISABLE_TINT.exe`: disable the tint
- `OpenKneeboard-RemoteControl-TOGGLE_TINT.exe`: enable the tint if disabled, disable it if enabled
- `OpenKneeboard-RemoteControl-RECENTER_VR.exe`: tell OpenKneeboard to recenter in virtual reality
- `OpenKneeboard-RemoteControl-SWAP_FIRST_TWO_VIEWS.exe`: when two kneeboards are enabled, switch active/inactive or left/right
- `OpenKneeboard-RemoteControl-TOGGLE_FORCE_ZOOM.exe`: in VR mode, toggle 'always zoom' on or off; if off, by default, OpenKneeboard will enlarge the kneeboard when you're looking directly at it.
- `OpenKneeboard-RemoteControl-HIDE.exe`: hide all views
- `OpenKneeboard-RemoteControl-SHOW.exe`: show all views
- `OpenKneeboard-RemoteControl-TOGGLE_VISIBILITY.exe`: show if hidden, hide if not hidden
- `OpenKneeboard-RemoteControl-PREVIOUS_PROFILE.exe`: if multiple profiles are configured, switch to the previous profile
- `OpenKneeboard-RemoteControl-NEXT_PROFILE.exe`: if multiple profiles are configured, switch to the next profile
- `OpenKneeboard-RemoteControl-PREVIOUS_BOOKMARK.exe`: if bookmarks are set, go the the previous bookmark
- `OpenKneeboard-RemoteControl-NEXT_BOOKMARK.exe`: if bookmarks are set, go to the next bookmark

These simple ones optionally take an argument to repeat the event - for example, to go forward 5 pages, run `...NEXT_PAGE.exe 5`.

**WARNING**: like changing profiles in the app, , changing profiles via a remote control discard all of the user's notes, bookmarks, and all other state, e.g. the current page for each tab, DCS radio history, etc.

## `OpenKneeboard-RemoteControl-SET_PROFILE.exe`

This must be ran with parameters:

- set profile by name: `OpenKneeboard-RemoteControl-SET_PROFILE.exe name "My Profile"`
- *removed in v1.9*: set profile by ID: `OpenKneeboard-RemoteControl-SET_PROFILE.exe id myprofile` (doesn't change if the profile is renamed)
- *added in v1.9* set profile by GUID : `OpenKneeboard-RemoteControl-SET_PROFILE.exe guid GUID_GOES_HERE` (doesn't change if the profile is renamed)


You can find IDs/GUIDs by looking in `Saved Games\OpenKneeboard\profiles.json` or the folder names inside `Saved Games\OpenKneeboard\profiles\`.

## `OpenKneeboard-RemoteControl-SET_TAB.exe`

This must be ran with parameters.

### Setting the current tab by name

This is the most straightforward approach, but will break if you rename the tabs, and may not do what you expect if there are multiple tabs with the same title.

Run with 'name TITLE', replacing the 'TITLE' with the tab title. Add double quotes if there are spaces or other special characters. For example:

    OpenKneeboard-RemoteControl-SET_TAB.exe name "Radio Log"

You can also jump to a specific page:

    OpenKneeboard-RemoteControl-SET_TAB.exe name "Radio Log" 123

If you don't specify a page number, or you specify '0', the tab will be selected without changing page within that tab.

### Setting the current tab by position

Run with `position POSITION` replacing 'POSITION' with the position of the tab you want to switch to, starting at 1. For example, to switch to the first tab:

    OpenKneeboard-RemoteControl-SET_TAB.exe position 1

### Setting the current tab by ID

This is more precise, but more work to set up.

An ID uniquely identifies a tab:
- if you have two tabs with the same title, they will have different IDs
- if you remove then re-add a tab, the re-added tab will have a different ID
- if you add the same folder/tab type to two different profiles, they will have different tab IDs
- if you add a tab to the default profile then restore a second profile's tab settings to default, the tab will have the same ID in both profiles

You can copy IDs between profiles when OpenKneeboard is not running to make them have the same ID - but if you do this, be careful to make sure you do not have two tabs with the same ID in the same profile. Tab IDs must stay in the same format (GUID).

First, you need to find the ID for the tab you want to select; these are in `Saved Games\OpenKneeboard\profiles\PROFILE_NAME_HERE\Tabs.json`.

For example, on my system, I see:

```
  {
    "ID": "{8e882d1e-de80-4b35-9388-f41a01d94a3d}",
    "Settings": {
      "MissionStartBehavior": "DrawHorizontalLine",
      "ShowTimestamps": true
    },
    "Title": "Radio Log",
    "Type": "DCSRadioLog"
  },
```

In my case, the 'Radio Log' tab has ID `{8e882d1e-de80-4b35-9388-f41a01d94a3d}`; I can select this tab with:

    OpenKneeboard-RemoteControl-SET_TAB.exe id "{8e882d1e-de80-4b35-9388-f41a01d94a3d}"

You can also jump to a specific page:

    OpenKneeboard-RemoteControl-SET_TAB.exe id "{8e882d1e-de80-4b35-9388-f41a01d94a3d}" 123

If you don't specify a page number, or you specify '0', the tab will be selected without changing page within that tab.

**The Radio Log tab will have a different ID on your installation.**

### Setting the tab/page for a particular view (kneeboard)

After the page number, you can also add:

- 0: change the current active view (default)
- 1: change the first view (e.g. 'left kneeboard')
- 2: change the second view (e.g. 'right kneeboard')
- 3...: etc

The 'active' view is usually the one you have directly looked at most recently.

For example:

    OpenKneeboard-RemoteControl-SET_TAB.exe name "Radio Log" 0 1

In this example, `0` is the page number (0 is "don't change"), and 1 specifies the first view.
