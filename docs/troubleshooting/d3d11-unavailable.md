---
parent: Troubleshooting
---

# Direct3D11 is Unusable

This is a Windows or GPU driver error; is is not an OpenKneeboard issue, and it can not be solved by OpenKneeboard.

Unless your situation is listed below, you need to ask your GPU manufacturer for support.

## Known issues

If *all* of these apply:

- You are running an NVidia driver 591.0 or later
- You have USB monitors or virtual monitors (e.g. WinWing Information Panels)
- You have Hardware Assisted GPU Scheduling (HAGS) disabled

The NVidia driver has a reduced limit for the number of concurrent applications. You can:

- try enabling HAGS
- disable or unplug your USB and virtual monitors
- try an older driver
- ask NVidia to undo this change or raise the limit
