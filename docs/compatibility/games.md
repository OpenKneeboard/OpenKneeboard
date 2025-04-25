---
title: Games
parent: Compatibility
---

# Game Compatibility

| Graphics API | Non-VR | OpenXR | Legacy Oculus API (OVR) | Legacy SteamVR API (OpenVR) |
| -------------|--------|--------|--------|---------|
| OpenGL       | ❌ | ❌ | ❌ | ✅ |
| Direct3D 11  | ✅ | ✅ | ✅ | ✅ |
| Direct3D 12  | ❌ | 🧪 | ☠️ | ✅ |
| Vulkan       | ❌ | 🧪 | ❌ | ✅ |

🧪: support is experimental; problems (including crashes) should be expected
☠️: support is being removed due to no known usage

OpenXR + Vulkan: the game must use `XR_KHR_vulkan_enable2` and the `xrCreateVulkanInstanceKHR()` + `xrCreateVulkanDeviceKHR()` functions.

Legacy Oculus API (OVR):

- where possible, OpenXR is *strongly* recommended instead
- only Link, Air Link, or Virtual Desktop are supported

Legacy SteamVR API (OpenVR): where possible, OpenXR is *strongly* recommended instead

32-bit games:

- in v1.8 and below, only OpenVR is supported for 32-bit games
- in v1.9 and above, 32-bit OpenXR games will also be supported.

## Common Games

| Game | Non-VR | Native OpenXR | Legacy Oculus SDK (OVR) | Legacy SteamVR API (OpenVR) |
|------|--------|---------------|------------|--------|
| DCS World | ✅ | ✅ | ✅ | ✅ |
| Falcon BMS | ✅ | n/a | n/a | ✅ |
| iRacing | ❌ | ✅ | ❌ | ✅ |
| MSFS 2020 - D3D11 | ✅ | ✅ | n/a | ✅ |
| MSFS 2020 - D3D12 | ❌ | 🧪 | n/a | ✅ |
| MSFS 2024 | ❌ | 🧪 | n/a | ✅ |
| BeamNG.drive | untested | ❌  | n/a | ✅ |

**BeamNG.drive:** as of April 4th, 2024, BeamNG.drive in VR is incompatible except when using SteamVR, because BeamNG.drive does not use `XR_KHR_vulkan_enable2` + `xrCreateVulkanInstanceKHR()` + `xrCreateVulkanDeviceKHR()`.

**iRacing:** support for other APIs is blocked by the anti-cheat software.

## OpenComposite

OpenKneeboard is not tested with OpenComposite, and issues will not be investigated.

## Advice

- For VR: use OpenXR if natively supported by the game - otherwise use OpenVR (SteamVR). OpenXR will give you the best performance and reliability
- OpenVR is the least likely mode to have compatibility issues, followed shortly by OpenXR. Non-VR and Oculus have significantly higher chances of issues
- Setting WinTab to 'invasive' in input settings also raises the chance of issues

## Anti-Cheat

While OpenKneeboard is not a cheat, it does hook into the games rendering and input pipelines, which over-eager anti-cheat systems may consider ban-worthy. While this is similar to how other overlays work (e.g. Steam and Discord), it is possible that using OpenKneeboard may lead to an anti-cheat ban.

As of March 19th, 2024, the developers have not received any reports of any users receiving any bans in any games, but this remains a theoretical possibility in the future.

Note that OpenKneeboard has **NO WARRANTY**; see [the LICENSE file](https://raw.githubusercontent.com/OpenKneeboard/OpenKneeboard/master/LICENSE) and [full text of the GPLv2](https://raw.githubusercontent.com/OpenKneeboard/OpenKneeboard/master/gpl-2.0.txt) for details.
