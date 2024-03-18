---
title: Games
parent: Compatibility
---

# Game Compatibility

| Graphics API | Non-VR | OpenXR | Oculus | SteamVR |
| -------------|--------|--------|--------|---------|
| OpenGL       | ❌ | ❌ | ❌ | ✅ |
| Direct3D 11  | ✅ | ✅ | ✅ | ✅ |
| Direct3D 12  | ❌ | ✅ | ✅ | ✅ |
| Vulkan       | ❌ | ✅ | ❌ | ✅ |

Oculus API: only Link or Air Link are supported; Virtual Desktop or other similar tools are only supported via SteamVR or OpenXR.

OpenXR + Vulkan: the game must use `XR_KHR_vulkan_enable2` and the `xrCreateVulkanInstanceKHR()` + `xrCreateVulkanDeviceKHR()` functions.

OpenKneeboard is not tested with OpenComposite, and issues will not be investigated.

## Common Games

| Game | Non-VR | Native OpenXR | Oculus SDK | OpenVR |
|------|--------|---------------|------------|--------|
| DCS World | ✅ | ✅ | ✅ | ✅ |
| Falcon BMS | ✅ | n/a | n/a | ✅ |
| MSFS 2020  | ✅ (D3D11 only) | ✅ | n/a | ✅ |