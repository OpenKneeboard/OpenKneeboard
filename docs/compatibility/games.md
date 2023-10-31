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
| Vulkan       | ❌ | 🧪 | ❌ | ✅ |

🧪: OpenKneeboard's support for Vulkan+OpenXR is currently experimental.

Oculus API: only Link or Air Link are supported; Virtual Desktop or other similar tools are only supported via SteamVR or OpenXR.

OpenKneeboard is not tested with OpenComposite, and issues will not be investigated.

## Common Games

| Game | Non-VR | Native OpenXR | Oculus SDK | SteamVR |
|------|--------|---------------|------------|---------|
| DCS World | ✅ | ✅ | ✅ | ✅ |
| Falcon BMS | ✅ | n/a | n/a | ✅ |
| MSFS 2020  | ✅ (D3D11 only) | ✅ | n/a | ✅ |