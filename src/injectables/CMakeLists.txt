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

ok_add_library(dxgi-hooks STATIC dxgi-offsets.c IDXGISwapChainHook.cpp)
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

ok_add_library(OpenKneeboard-nonvr-d3d11 MODULE NonVRD3D11Kneeboard.cpp)
target_link_libraries(
  OpenKneeboard-nonvr-d3d11
  PRIVATE
  dxgi-hooks
  detours-ext
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-tracing
  OpenKneeboard-version
  OpenKneeboard-D3D11
  OpenKneeboard-SHM
  OpenKneeboard-SHM-Client-D3D11
  OpenKneeboard-DllLoadWatcher)

ok_add_library(
  OpenKneeboard-oculus-d3d11
  MODULE
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
  OpenKneeboard-tracing
  OpenKneeboard-version
  OpenKneeboard-D3D11
  OpenKneeboard-SHM-Client-D3D11
  oculus-hooks
  oculus-sdk-headers)

configure_file(
  OpenKneeboard-OpenXR.in.def
  OpenKneeboard-OpenXR${BUILD_BITNESS}.def
  @ONLY
  NEWLINE_STYLE UNIX
)
ok_add_library(
  OpenKneeboard-OpenXR
  MODULE
  DUALARCH
  OpenKneeboard-OpenXR${BUILD_BITNESS}.def
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
  OpenKneeboard-StateMachine
  OpenKneeboard-VRKneeboard
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-scope_exit
  OpenKneeboard-shims
  OpenKneeboard-version
)

configure_file(
  OpenKneeboard-OpenXR.in.json
  OpenKneeboard-OpenXR.intermediate.json
  NEWLINE_STYLE UNIX
)

# Keep the un-suffixed name to keep compatibility with previous installations
set(64BIT_OPENXR_JSON "${BUILD_OUT_BINDIR}/OpenKneeboard-OpenXR.json")
set(32BIT_OPENXR_JSON "${BUILD_OUT_BINDIR}/OpenKneeboard-OpenXR32.json")
if(BUILD_IS_32BIT)
  set(OPENXR_JSON_OUT "${32BIT_OPENXR_JSON}")
else()
  set(OPENXR_JSON_OUT "${64BIT_OPENXR_JSON}")
endif()
file(
  GENERATE
  OUTPUT "${OPENXR_JSON_OUT}"
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/OpenKneeboard-OpenXR.intermediate.json"
  NEWLINE_STYLE UNIX
)

ok_add_library(OpenKneeboard-AutoDetect MODULE InjectionBootstrapper.cpp)
target_link_libraries(
  OpenKneeboard-AutoDetect
  PRIVATE
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-tracing
  OpenKneeboard-version
  detours-ext
  dxgi-hooks
  oculus-hooks
  openvr-hooks)

ok_add_library(OpenKneeboard-WindowCaptureHook MODULE DUALARCH WindowCaptureHook.cpp)
target_link_libraries(
  OpenKneeboard-WindowCaptureHook
  PRIVATE
  OpenKneeboard-config
  OpenKneeboard-tracing
  detours-ext
)
set_target_properties(
  OpenKneeboard-WindowCaptureHook
  PROPERTIES

  # Needed to avoid C function name mangling on 32-bit builds
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
