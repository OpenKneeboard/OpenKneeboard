---
parent: Features
---

# DCS Integration

OpenKneeboard supports integration with DCS, including:

- a clock in OpenKneeboard's footer showing the current mission time, both in local time and zulu time
- various tabs showing files or data related to the current mission

## Supported Files (Pages)

OpenKneeboard aims to look for kneeboard files in the same location as DCS's built-in kneeboard, however *it is only able to display image files*; it is not able to display any kneeboard pages written in Lua; for example, these **are not supported**, and are unlikely to be supported in any future version:

- generated charts
- carrier landing grading
- built-in kneeboard page that control the aircraft or its' payload

## Configuration

DCS must be listed in OpenKneeboard Settings -> Games, and the 'Saved Games' path must be correct. If you have any trouble, check [the troubleshooting guide](../troubleshooting/dcs-tabs.md); if the steps there don't resolve your problems, check out [Getting Help](../getting-help.md).

The clock and DCS tabs are shown by default; if you have removed the DCS tabs, you can re-add them by opening OpenKneeboard Settings -> Tabs, then either:

- restore the default tabs
- click 'add tab', and pick the tab type you want to add

## The 'DCS Aircraft' Tab

This will show any *image files* for the current aircraft, from:

- ***`DCS saved games path`***`\KNEEBOARD\`***`aircraft`***
- ***`DCS installation path`***`\Mods\aircraft\`***`module`***`\Cockpit\KNEEBOARD\pages`
- ***`DCS installation path`***`\Mods\aircraft\`***`module`***`\Cockpit\Scripts\KNEEBOARD\pages`

'***`aircraft`***' and '***`module`***' are usually the same; exceptions include:
- The `F-16C` module in the DCS installation corresponds to the `F-16C_60` aircraft in *Saved Games*
- The `FA-18C` module in the DCS installation corresponds to the `FA-18C_hornet` aircraft in *Saved Games*
- The `AH-64D` module in the DCS installation corresponds to the `AH-64D_BLK_II` aircraft in *Saved Games*

For example, I put my A-10C kneeboard images in `C:\Users\fred\Saved Games\DCS.openbeta\KNEEBOARD\A-10C_2`.

These paths are the same as the paths that DCS itself uses.

## The 'DCS Briefing' Tab

This will show objectives, briefing images, briefing text, and weather from the current `.miz` file - the same content you see before starting the mission. It will also show the coordinates and magnetic variation at the bullseye, along with the A-10C CDU wind calculations at the bullseye (not your initial starting point) if you're flying an A-10C.

It currently does not show forces, airfields, or frequencies.

This tab is mostly useful for missions or campaigns which do not include custom kneeboard images - which will appear in [the 'DCS Mission' tab'](#the-dcs-mission-tab) - but the weather and bullseye information here can be useful even with kneeboard images.

## The 'DCS Mission' Tab

This will show any custom kneeboard images inside the `.miz` file; the `.miz` file is essential a zip file, and this tab shows any files that the mission designer has put inside the `KNEEBOARD/IMAGES` subfolder.

This could be empty, the same as the briefing images, or entirely different - this is up to the mission designer, and matches DCS's built-in kneeboard.

## The 'DCS Radio Log' Tab

This shows all radio or mission messages that include text, similar to the 'Message History' feature within DCS; it does not log any spoken radio messages from voice chat.

This is particularly useful for AI or scripted campaign JTAC 9-lines.

## The 'DCS Theater' Tab

This will show any *image files* from:

- ***`DCS saved games path`***`\KNEEBOARD\`***`theater`***
- ***`DCS installation path`***`\Mods\terrains\`***`theater`***`\Kneeboard`

**Most DCS maps do not include any kneeboard images**.

DCS's built-in kneeboard will only load kneeboard images for maps from the DCS installation path; OpenKneeboard also will load any from saved games for convenience.
