# Using an Elgato StreamDeck with OpenKneeboard

A StreamDeck can be used for basic control, e.g. selectinging pages and tabs, or showing and hiding the kneeboard. There are two ways to do this:

## HotKeys

Use the System -> HotKey StreamDeck action, and assign a keystroke combination that does not conflict with anything else you're using keystrokes for (including your in-game keybinds).

F17-24 are particularly useful for this, as they do not exist on most keyboards, fully supported by DirectInput,  and are rarely used by other software:

![binding F17 through the StreamDeck UI](screenshots/streamdeck-hotkey.png)

Once this is done, you can bind it through Settings -> DirectInput -> Keyboard in OpenKneeboard:

![screenshot of F17 bound to show/hide in OpenKneeboard](screenshots/openkneeboard-bound-f17.png)

## Open Application

This is a bit more initial setup, but means you don't need to worry about finding a unique key combination.

The `OpenKneeboard-RelWithDebInfo-RemoteControl` zip from [the latest release](https://github.com/fredemmott/OpenKneeboard/releases/latest) contains several remote control executables that can be used with StreamDeck's "Open" action:

![OpenKneeboard-Remote-PREVIOUS_PAGE.exe, -NEXT_PAGE.exe, -NEXT_TAB.exe, etc](screenshots/remote-controls.png)

Extract these somewhere to keep.

Use the System -> Open StreamDeck application, and browse to the application you want; for example, if you want a StreamDeck key to move to the next kneeboard page, select `OpenKneeboard-RemoteControl-NEXT_PAGE.exe`:

![Screenshot of Elgato software with Next Page remote control](screenshots/streamdeck-open.png)
