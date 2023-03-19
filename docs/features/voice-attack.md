---
parent: Features
---

# VoiceAttack

OpenKneeboard includes [remote control executables](./remote-controls.md) in `C:\Program Files\OpenKneeboard\utilities`, which can be used with VoiceAttack:

## Configuration

Select "When this command executes, do the following sequence:" -> 'Other' -> 'Windows' -> 'Run an Application':

![Screenshot of the 'Windows' menu open and 'Run an Application' selected](../screenshots/voice-attack-run-an-application.png)

Then, select the remote control you want from `C:\Program Files\OpenKneeboard\utilities`:

![Screenshot of the HIDE exe selected](../screenshots/voice-attack-no-args.png)

For remote controls that take parameters, put them in the 'With these parameters' box - for example:

> name "Radio Log"

... or ...

> id "{8e882d1e-de80-4b35-9388-f41a01d94a3d}"

This example ID will not be valid on your installation.

![Screenshot of setting the tab to "Radio Log"](../screenshots/voice-attack-args.png)