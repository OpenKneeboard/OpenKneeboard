ok_add_library(detours-ext STATIC detours-ext.cpp)
target_link_libraries(detours-ext PUBLIC detours OpenKneeboard-dprint)

ok_add_library(OpenKneeboard-DllLoadWatcher STATIC DllLoadWatcher.cpp)
target_link_libraries(
  OpenKneeboard-DllLoadWatcher
  PRIVATE
  detours-ext
  OpenKneeboard-dprint)

ok_add_library(OpenKneeboard-function-patterns STATIC function-patterns.cpp)
target_link_libraries(
  OpenKneeboard-function-patterns
  PRIVATE
  OpenKneeboard-dprint)

ok_add_library(OpenKneeboard-FindMainWindow STATIC FindMainWindow.cpp)

ok_add_library(dxgi-hooks STATIC dxgi-offsets.c IDXGISwapChainPresentHook.cpp)
target_link_libraries(
  dxgi-hooks
  PRIVATE
  detours-ext
  OpenKneeboard-dprint
  OpenKneeboard-function-patterns
  OpenKneeboard-FindMainWindow)

ok_add_library(d3d12-hooks STATIC d3d12-offsets.c ID3D12CommandQueueExecuteCommandListsHook.cpp)
target_link_libraries(d3d12-hooks PRIVATE detours-ext OpenKneeboard-dprint System::D3d12)

ok_add_library(OpenKneeboard-ScopedRWX STATIC ScopedRWX.cpp)

ok_add_library(
  oculus-hooks
  STATIC
  OculusEndFrameHook.cpp
  OculusKneeboard.cpp
  OVRProxy.cpp)
target_link_libraries(
  oculus-hooks
  PUBLIC
  OpenKneeboard-DllLoadWatcher
  OpenKneeboard-RayIntersectsRect
  OpenKneeboard-SHM
  OpenKneeboard-VRKneeboard
  OpenKneeboard-config
  OpenKneeboard-dprint
  ThirdParty::DirectXTK
  detours-ext
  oculus-sdk-headers
)

ok_add_library(openvr-hooks STATIC IVRCompositorWaitGetPosesHook.cpp)
target_link_libraries(
  openvr-hooks
  PUBLIC
  detours-ext
  OpenKneeboard-DllLoadWatcher
  OpenKneeboard-ScopedRWX
  openvr-headers
  OpenKneeboard-dprint)

add_module_library(OpenKneeboard-nonvr-d3d11 NonVRD3D11Kneeboard.cpp)
target_link_libraries(
  OpenKneeboard-nonvr-d3d11
  PRIVATE
  dxgi-hooks
  detours-ext
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-version
  OpenKneeboard-D3D11
  OpenKneeboard-SHM
  OpenKneeboard-SHM-Client-D3D11
  OpenKneeboard-DllLoadWatcher)

add_module_library(
  OpenKneeboard-oculus-d3d11
  OculusD3D11Kneeboard.cpp
  OculusD3D11Kneeboard_DllMain.cpp
)
target_link_libraries(
  OpenKneeboard-oculus-d3d11
  PRIVATE
  dxgi-hooks
  detours-ext
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-version
  OpenKneeboard-D3D11
  OpenKneeboard-SHM-Client-D3D11
  oculus-hooks
  oculus-sdk-headers)

add_module_library(
  OpenKneeboard-oculus-d3d12
  OculusD3D12Kneeboard.cpp
  OculusD3D12Kneeboard_DllMain.cpp
)
target_link_libraries(
  OpenKneeboard-oculus-d3d12
  PRIVATE
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-version
  OpenKneeboard-ScopedRWX
  OpenKneeboard-SHM-Client-D3D12
  d3d12-hooks
  detours-ext
  oculus-hooks
  oculus-sdk-headers)

add_module_library(
  OpenKneeboard-OpenXR
  OpenXRKneeboard.cpp
  OpenXRD3D11Kneeboard.cpp
  OpenXRD3D12Kneeboard.cpp
  OpenXRVulkanKneeboard.cpp
  OpenXRNext.cpp
)
target_link_libraries(
  OpenKneeboard-OpenXR
  PRIVATE
  ThirdParty::OpenXR
  ThirdParty::DirectXTK
  ThirdParty::DirectXTK12
  ThirdParty::VulkanHeaders
  OpenKneeboard-D3D11
  OpenKneeboard-D3D12
  OpenKneeboard-Vulkan
  OpenKneeboard-Elevation
  OpenKneeboard-RayIntersectsRect
  OpenKneeboard-SHM
  OpenKneeboard-SHM-Client-D3D11
  OpenKneeboard-SHM-Client-D3D12
  OpenKneeboard-SHM-Client-Vulkan
  OpenKneeboard-VRKneeboard
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-scope_guard
  OpenKneeboard-shims
  OpenKneeboard-version
)

file(
  GENERATE
  OUTPUT "OpenKneeboard-OpenXR.json"
  INPUT "OpenKneeboard-OpenXR.json.in"
  NEWLINE_STYLE UNIX
)

add_module_library(OpenKneeboard-AutoDetect InjectionBootstrapper.cpp)
target_link_libraries(
  OpenKneeboard-AutoDetect
  PRIVATE
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-version
  detours-ext
  dxgi-hooks
  oculus-hooks
  openvr-hooks)

add_module_library(OpenKneeboard-TabletProxy TabletProxy.cpp)
target_link_libraries(
  OpenKneeboard-TabletProxy
  PRIVATE
  detours-ext
  OpenKneeboard-dprint
  OpenKneeboard-FindMainWindow
  OpenKneeboard-GetMainHWND
  OpenKneeboard-Wintab
  OpenKneeboard-version
)

add_module_library(OpenKneeboard-WindowCaptureHook WindowCaptureHook.cpp)
target_link_libraries(
  OpenKneeboard-WindowCaptureHook
  PRIVATE
  OpenKneeboard-config
  _libheaders
  detours-ext
)
set_target_properties(
  OpenKneeboard-WindowCaptureHook
  PROPERTIES

  # Needed to avoid C function name mangling on 32-bit builds
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
