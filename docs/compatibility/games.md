---
title: Games
parent: Compatibility
---

# Game Compatibility

| Graphics API | Non-VR | OpenXR | Oculus | OpenVR (SteamVR) |
| -------------|--------|--------|--------|---------|
| OpenGL       | ❌ | ❌ | ❌ | ✅ |
| Direct3D 11  | ✅ | ✅ | ✅ | ✅ |
| Direct3D 12  | ❌ | ✅ | ✅ | ✅ |
| Vulkan       | ❌ | ✅ | ❌ | ✅ |

Oculus API: only Link or Air Link are supported; Virtual Desktop or other similar tools are only supported via SteamVR or OpenXR.

OpenXR + Vulkan: the game must use `XR_KHR_vulkan_enable2` and the `xrCreateVulkanInstanceKHR()` + `xrCreateVulkanDeviceKHR()` functions.

## Common Games

| Game | Non-VR | Native OpenXR | Oculus SDK | OpenVR (SteamVR) |
|------|--------|---------------|------------|--------|
| DCS World | ✅ | ✅ | ✅ | ✅ |
| Falcon BMS | ✅ | n/a | n/a | ✅ |
| iRacing | untested | ✅ | untested | ✅ |
| MSFS 2020  | ✅ (D3D11 only) | ✅ | n/a | ✅ |

## Advice

- For VR: use OpenXR if possible - otherwise use OpenVR (SteamVR). OpenXR will give you the best performance and reliability
- OpenVR is the least likely mode to have compatibility issues, followed shortly by OpenXR. Non-VR and Oculus have significantly higher chances of issues
- Setting WinTab to 'invasive' in input settings also raises the chance of issues

## Anti-Cheat

While OpenKneeboard is not a cheat, it does hook into the games rendering and input pipelines, which over-eager anti-cheat systems may consider ban-worthy. While this is similar to how other overlays work (e.g. Steam and Discord), it is possible that using OpenKneeboard may lead to an anti-cheat ban.

As of March 19th, 2024, the developers have not received any reports of any users receiving any bans in any games, but this remains a theoretical possibility in the future.

Note that OpenKneeboard has **NO WARRANTY**; see [the LICENSE file](https://raw.githubusercontent.com/OpenKneeboard/OpenKneeboard/master/LICENSE) and [full text of the GPLv2](https://raw.githubusercontent.com/OpenKneeboard/OpenKneeboard/master/gpl-2.0.txt) for details.

## OpenComposite

OpenKneeboard is not tested with OpenComposite, and issues will not be investigated.