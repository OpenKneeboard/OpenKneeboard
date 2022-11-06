# Crashes on startup, or no window visible

When you start OpenKneeboard, you should see a window like this:

![A normal Windows app](../screenshots/config-app.png)

If you do not, OpenKneeboard is crashing on startup.

## Tablet drivers

XP-Pen's WinTab driver crashes OpenKneeboard on startup; either XP-Pen's drivers must be completely uninstalled, or `C:\Windows\System32\WinTab32.dll` must be backed up then deleted (if from XP-Pen). This may break other applications you are using your tablet with.

`WinTab32.dll` is not a Windows component; each graphics tablet manufacturer (Huion, Wacom, etc) provides their own version.

If you do not have a graphics tablet (Wacom, Huion etc - not a tablet computer like an iPad or MS Surface tablet) but do have a `C:\Windows\System32\WinTab32.dll`, back up and delete this file; it should only be present on systems with a graphics tablet driver installed.

## Corrupt settings

Try renaming `Saved Games\OpenKneeboard` to anything else; this will remove your OpenKneeboard settings.

## Reporting crashes

Please do not report crashes if you have:
- an XP-Pen tablet driver installed
- a Gaomon tablet driver installed
- `C:\Windows\System32\WinTab32.dll` with that specific capitalization of `WinTab`

When reporting crashes:
- please report via [GitHub issues](https://go.openkneeboard.com/issues)
- if you have OpenKneeboard .dmp files in `Saved Games\OpenKneeboard`, please attach the latest one to the issue
- otherwise, you may have them in `%LOCALAPPDATA%\CrashDumps`; if so, please attach the latest one
- otherwise, they can be enabled by downloading and running either:
  - [enable-full-dumps.reg](https://github.com/OpenKneeboard/OpenKneeboard/raw/master/docs/enable-full-dumps.reg): this may create files > 200mb in size
  - [enable-mini-dumps.reg](https://raw.githubusercontent.com/OpenKneeboard/OpenKneeboard/master/docs/enable-mini-dumps.reg): this will create much smaller files, but they are relatively unlikely to be sufficient for diagnosing the problem. They are however much better than no dump if sharing 200mb+ files is not practical for you.

If your crash was fixed by renaming `Saved Games\OpenKneeboard` , please also attach a zip of that folder, excluding any .dmp files.
