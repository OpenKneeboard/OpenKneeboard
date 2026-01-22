ok_add_library(detours-ext STATIC detours-ext.cpp)
target_link_libraries(detours-ext PUBLIC ThirdParty::detours OpenKneeboard-dprint)

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
