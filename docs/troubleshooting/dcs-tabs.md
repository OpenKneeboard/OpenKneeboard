# The DCS Tabs

## None of the tabs ever show anything

1. Check if the Theater tab loads for Caucasus or NTTR; if so, check a different section.
2. Check that DCS is in OpenKneeboard's Games list inside settings, and that the Saved Games path is correct
3. Install the latest [Microsoft Visual C++ Redistributables](https://aka.ms/vs/17/release/vc_redist.x64.exe)

## The theater tabs only loads for NTTR and Caucasus

DCS World does not include kneeboard images for other maps; OpenKneeboard does not currently generate charts.

## Built-in aircraft kneeboard pages are not loaded

Currently, the aircraft tab only loads kneeboard from the DCS Saved Games 'KNEEBOARD\` folder, not from the DCS installation; you can
copy the built-in images there from the `KNEEBOARD` folders inside `Mods\Aircraft`. Combining both will likely be supported in a future release.

Dynamic (Lua) kneeboard pages are not supported at all, and are unlikely to be supported in the future. This
includes most of the kneeboard pages for the F14, F16, and F18.
