---
parent: Features
---

# Window Capture

Window Capture tabs embed a window within OpenKneeboard; this can be used to bring a window into VR:

![Google Chrome with Youtube](../screenshots/window-capture.png)

If you have [a graphics tablet](./graphics-tablets.md), you can use that to interact with the captured window while in game.

If this is all you use OpenKneeboard for, you might want to [hide OpenKneeboard's header and footer](../faq/index.md#how-do-i-remove-the-header-or-footer-borders).

If you encounter problems, check [the troubleshooting guide](../troubleshooting/window-capture-compatibility.md); if there steps there don't resolve your problems, check out [Getting Help](../getting-help.md).

## Excluding the title bar and borders

Turn on the 'Capture client area' option in the tab settings; if this doesn't work, ask the developers of the window you're capturing to support `GetClientRect()` - you may want to point them to [the developer FAQ entry](../faq/third-party-developers.md#how-can-people-capture-my-app-without-the-title-bar).

## Capturing only part of the window (cropping)

If you want to exclude the title bar and borders, see above.

There is no more general feature in OpenKneeboard; any updates will be [on the GitHub issue](https://github.com/OpenKneeboard/OpenKneeboard/issues/595).