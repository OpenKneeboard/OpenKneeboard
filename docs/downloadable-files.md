# Downloadable Files

You probably want `OpenKneeboard[-Version]-RelWithDebInfo.msix`. Huion and StreamDeck users may also want `OpenKneeboard[-Version]-RelWithDebInfo-RemoteControl.zip`; see the [Huion](huion.md] and [StreamDeck](streamdeck.md) notes for more information.

Everything is built twice, and available in two forms:

* RelWithDebInfo - **fast, good for users**
* Debug - **slow, primarily for developers**; contains extra safety checks and verbose logging. The safety checks do not make it more likely to work, but if there is a problem, they easier to find out why it doesn't work.

Within these categories, there are several files:
* `OpenKneeboard[-Version]-(RelWithDebInfo|Debug).msix`: A Windows App package installer. This is probably what you want. It contains the main OpenKneeboard application and everything it depends on.
* `OpenKneeboard[-Version]-(RelWithDebInfo|Debug)-RemoteControl`: Executables for controlling OpenKneeboard, e.g. switching pages/tabs. These are useful for integrating with other applications, like the Huion drivers or Elgato's StreamDeck software.
* `OpenKneeboard[-Version]-(RelWithDebInfo|Debug)`: The same executables as the `.msix`, but in a `.zip` file. Not really needed - use this if you prefer not to use installers.
* `OpenKneeboard[-Version]-(RelWithDebInfo|Debug)-Symbols`: Extra files that are useful for debugging crashes. For example, they can be used to translate things like 'crashed at `0x12345678` to 'crashed in `some_function()` at line 123 of `somefile.cpp`'.
