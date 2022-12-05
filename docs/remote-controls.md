# 'RemoteControl' exe files

These are included in the 'utilities' zip, and can be used to control OpenKneeboard from other programs, e.g.:

- the Elgato StreamDeck software via 'System' -> 'Open'
- Huion tablets by binding to an executable
- VoiceAttack via "When this command executes, do the following sequence:" -> 'Other' -> 'Windows' -> 'Run an Application'

Most of these are relatively self-explanatory:

- `OpenKneeboard-RemoteControl-PREVIOUS_TAB.exe`: tell OpenKneeboard to go to the previous tab
- `OpenKneeboard-RemoteControl-NEXT_TAB.exe`: tell OpenKneeboard to go to the next tab
- `OpenKneeboard-RemoteControl-PREVIOUS_PAGE.exe`: tell OpenKneeboard to go to the previous page in the current tab
- `OpenKneeboard-RemoteControl-NEXT_PAGE.exe`: tell OpenKneeboard to go to the next page in the current tab
- `OpenKneeboard-RemoteControl-RECENTER_VR.exe`: tell OpenKneeboard to recenter in virtual reality
- `OpenKneeboard-RemoteControl-SWITCH_KNEEBOARDS.exe`: when two kneeboards are enabled, switch active/inactive or left/right
- `OpenKneeboard-RemoteControl-TOGGLE_FORCE_ZOOM.exe`: in VR mode, toggle 'always zoom' on or off; if off, by default, OpenKneeboard will enlarge the kneeboard when you're looking directly at it.
- `OpenKneeboard-RemoteControl-TOGGLE_VISIBILITY.exe`: show/hide the kneeboard in-game

## `OpenKneeboard-RemoteControl-SET_TAB.exe` (v1.3-beta3 and above)

This must be ran with parameters.

### Setting the current tab by name

This is the most straightforward approach, but will break if you rename the tabs, and may not do what you expect if there are multiple tabs with the same title.

Run with 'name TITLE', replacing the 'TITLE' with the tab title. Add double quotes if there are spaces or other special characters. For example:

    OpenKneeboard-RemoteControl-SET_TAB.exe name "Radio Log"

You can also jump to a specific page:

    OpenKneeboard-RemoteControl-SET_TAB.exe name "Radio Log" 123

If you don't specify a page number, or you specify '0', the tab will be selected without changing page within that tab.

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

### Setting the tab/page for a particular kneeboard (dual kneeboards, v1.3-beta4 and above)

After the page number, you can also add:

- 0: change the current active kneeboard (default)
- 1: change the primary (left) kneeboard
- 2: change the secondary (right) kneeboard

For example:

    OpenKneeboard-RemoteControl-SET_TAB.exe name "Radio Log" 0 1

In this example, `0` is the page number (0 is "don't change"), and 1 specifies the primary kneeboard.

### StreamDeck

Add an 'Open' action, select the SET_TAB exe, then add a space then the arguments at the end of the 'App / File' box, after the closing quotation marks. For example:

> "C:\Foo\OpenKneeboard-RemoteControl-SET_TAB.exe" name "Radio Log"

... or ...

> "C:\Foo\OpenKneeboard-RemoteControl-SET_TAB.exe" id "{8e882d1e-de80-4b35-9388-f41a01d94a3d}"

This example ID will not be valid on your installation.

### Voice attack

In the 'Run an Application' dialog, enter the parameters - separated by spaces - in the 'With these parameters' box - for example:

> name "Radio Log"

... or ...

> id "{8e882d1e-de80-4b35-9388-f41a01d94a3d}"

This example ID will not be valid on your installation.
