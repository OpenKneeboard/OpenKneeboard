ok_add_library(_libheaders INTERFACE)
target_include_directories(_libheaders INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/OpenKneeboard/config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/OpenKneeboard/config.h"
  @ONLY
)
ok_add_library(OpenKneeboard-config INTERFACE)
target_include_directories(
  OpenKneeboard-config
  INTERFACE
  ${CMAKE_CURRENT_BINARY_DIR}/include
)

ok_add_library(OpenKneeboard-ThreadGuard STATIC ThreadGuard.cpp)
target_link_libraries(OpenKneeboard-ThreadGuard PUBLIC _libheaders)
target_link_libraries(OpenKneeboard-ThreadGuard PRIVATE OpenKneeboard-dprint)

ok_add_library(OpenKneeboard-GetMainHWND STATIC GetMainHWND.cpp)
target_link_libraries(
  OpenKneeboard-GetMainHWND
  PUBLIC
  _libheaders
)
target_link_libraries(
  OpenKneeboard-GetMainHWND
  PRIVATE
  OpenKneeboard-config
  OpenKneeboard-shims
)

ok_add_library(OpenKneeboard-RunSubprocessAsync STATIC RunSubprocessAsync.cpp)
target_link_libraries(
  OpenKneeboard-RunSubprocessAsync
  PUBLIC
  OpenKneeboard-shims
)
target_link_libraries(
  OpenKneeboard-RunSubprocessAsync
  PRIVATE
  OpenKneeboard-dprint
)

ok_add_library(OpenKneeboard-GetSystemColor STATIC GetSystemColor.cpp)
target_link_libraries(
  OpenKneeboard-GetSystemColor
  PUBLIC
  _libheaders)

ok_add_library(OpenKneeboard-FilesDiffer STATIC FilesDiffer.cpp)
target_link_libraries(
  OpenKneeboard-FilesDiffer
  PUBLIC
  _libheaders)

ok_add_library(OpenKneeboard-handles INTERFACE)
target_link_libraries(
  OpenKneeboard-handles
  INTERFACE
  _libheaders
  ThirdParty::OutPtr)

ok_add_library(OpenKneeboard-Elevation STATIC Elevation.cpp)
target_link_libraries(
  OpenKneeboard-Elevation
  PUBLIC
  _libheaders
)
target_link_libraries(
  OpenKneeboard-Elevation
  PRIVATE
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-scope_guard
  ThirdParty::CppWinRT
  System::Shell32
)

ok_add_library(OpenKneeboard-UTF8 STATIC utf8.cpp)
target_link_libraries(
  OpenKneeboard-UTF8
  PUBLIC
  ThirdParty::JSON
  _libheaders
)

ok_add_library(OpenKneeboard-PDFNavigation STATIC PDFNavigation.cpp)
target_link_libraries(
  OpenKneeboard-PDFNavigation
  PUBLIC
  _libheaders
  OpenKneeboard-shims
)
target_link_libraries(
  OpenKneeboard-PDFNavigation
  PRIVATE
  OpenKneeboard-DebugTimer
  OpenKneeboard-UTF8
  OpenKneeboard-dprint
  ThirdParty::QPDF
)

ok_add_library(OpenKneeboard-DebugTimer STATIC DebugTimer.cpp)
target_link_libraries(
  OpenKneeboard-DebugTimer
  PRIVATE
  OpenKneeboard-dprint
)
target_link_libraries(
  OpenKneeboard-DebugTimer
  PUBLIC
  _libheaders
)

ok_add_library(
  OpenKneeboard-D3D11
  STATIC
  D3D11.cpp
  D3D11/Renderer.cpp
)
target_link_libraries(
  OpenKneeboard-D3D11
  PUBLIC
  System::D3d11
  OpenKneeboard-shims
  ThirdParty::DirectXTK
  ThirdParty::CppWinRT
  _libheaders
  PRIVATE
  OpenKneeboard-dprint
  System::Dxguid
)

ok_add_library(
  OpenKneeboard-D3D12
  STATIC
  D3D12.cpp
  D3D12/Renderer.cpp
)
target_link_libraries(
  OpenKneeboard-D3D12
  PUBLIC
  System::D3d12
  OpenKneeboard-shims
  ThirdParty::DirectXTK12
  ThirdParty::CppWinRT
  _libheaders
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-RenderDoc
  System::Dxguid
)

ok_add_library(
  OpenKneeboard-Vulkan
  STATIC
  Vulkan.cpp
  Vulkan/SpriteBatch.cpp)
target_link_libraries(
  OpenKneeboard-Vulkan
  PUBLIC
  OpenKneeboard-dprint
  ThirdParty::VulkanHeaders
  _libheaders
  OpenKneeboard-SpriteBatch-SPIRV
)

ok_add_library(OpenKneeboard-DXResources STATIC DXResources.cpp)
target_link_libraries(
  OpenKneeboard-DXResources
  PUBLIC
  _libheaders
  OpenKneeboard-D3D11
  PRIVATE
  OpenKneeboard-dprint
  System::Gdi32
)

set(RUNTIME_FILES_CPP "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/RuntimeFiles.cpp")
file(GENERATE OUTPUT "${RUNTIME_FILES_CPP}" INPUT RuntimeFiles.cpp.in)
ok_add_library(
  OpenKneeboard-RuntimeFiles
  STATIC
  "${RUNTIME_FILES_CPP}"
  RuntimeFilesGetInstallationDirectory.cpp
)
target_link_libraries(OpenKneeboard-RuntimeFiles PUBLIC _libheaders)
target_link_libraries(
  OpenKneeboard-RuntimeFiles
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-Filesystem
)

ok_add_library(
  OpenKneeboard-SHM
  STATIC
  SHM.cpp
  SHM/ActiveConsumers.cpp
  FlatConfig.cpp
)
target_link_libraries(
  OpenKneeboard-SHM
  PUBLIC
  OpenKneeboard-dprint
  _libheaders
  PRIVATE
  OpenKneeboard-Elevation
  OpenKneeboard-config
  OpenKneeboard-shims
  OpenKneeboard-version
)

ok_add_library(
  OpenKneeboard-SHM-Client-D3D11
  STATIC
  SHM/D3D11.cpp
)
target_link_libraries(
  OpenKneeboard-SHM-Client-D3D11
  PUBLIC
  OpenKneeboard-shims
  OpenKneeboard-SHM
  System::D3d11
  ThirdParty::CppWinRT
  ThirdParty::DirectXTK
  PRIVATE
  OpenKneeboard-dprint
)

ok_add_library(
  OpenKneeboard-SHM-Client-D3D12
  STATIC
  SHM/D3D12.cpp
)
target_link_libraries(
  OpenKneeboard-SHM-Client-D3D12
  PUBLIC
  OpenKneeboard-shims
  System::D3d12
  ThirdParty::CppWinRT
  ThirdParty::DirectXTK12
  PRIVATE
  OpenKneeboard-dprint
)

ok_add_library(
  OpenKneeboard-SHM-Client-Vulkan
  STATIC
  SHM/Vulkan.cpp
)
target_link_libraries(
  OpenKneeboard-SHM-Client-Vulkan
  PUBLIC
  OpenKneeboard-SHM-Client-D3D11
  OpenKneeboard-Vulkan
  OpenKneeboard-shims
  ThirdParty::CppWinRT
  ThirdParty::VulkanHeaders
)

ok_add_library(OpenKneeboard-dprint STATIC dprint.cpp)
target_link_libraries(
  OpenKneeboard-dprint
  PUBLIC
  _libheaders
  OpenKneeboard-config
)
target_link_libraries(OpenKneeboard-dprint
  PRIVATE
  OpenKneeboard-scope_guard
)

ok_add_library(OpenKneeboard-consolelib STATIC ConsoleLoopCondition.cpp)
target_link_libraries(OpenKneeboard-consolelib PUBLIC _libheaders)

ok_add_library(OpenKneeboard-GameEvent STATIC GameEvent.cpp)
target_link_libraries(OpenKneeboard-GameEvent PRIVATE OpenKneeboard-config OpenKneeboard-dprint)
target_link_libraries(OpenKneeboard-GameEvent PUBLIC _libheaders OpenKneeboard-UTF8 OpenKneeboard-json)

ok_add_library(OpenKneeboard-D2DErrorRenderer STATIC D2DErrorRenderer.cpp)
target_link_libraries(
  OpenKneeboard-D2DErrorRenderer
  PUBLIC
  OpenKneeboard-DXResources
  OpenKneeboard-UTF8
  OpenKneeboard-config
  System::D2d1
  System::Dwrite
  _libheaders)

ok_add_library(OpenKneeboard-RayIntersectsRect STATIC RayIntersectsRect.cpp)
target_link_libraries(
  OpenKneeboard-RayIntersectsRect
  PUBLIC
  _libheaders
  ThirdParty::DirectXTK)
target_link_libraries(
  OpenKneeboard-RayIntersectsRect
  PRIVATE
  OpenKneeboard-dprint)

ok_add_library(OpenKneeboard-scope_guard STATIC scope_guard.cpp)
target_link_libraries(
  OpenKneeboard-scope_guard
  PUBLIC
  _libheaders)

ok_add_library(OpenKneeboard-Wintab
  STATIC
  WintabTablet.cpp
)
target_link_libraries(
  OpenKneeboard-Wintab
  PRIVATE
  _libheaders
  ThirdParty::Wintab
  OpenKneeboard-dprint
  OpenKneeboard-UTF8
)

ok_add_library(OpenKneeboard-VRKneeboard STATIC VRKneeboard.cpp)
target_link_libraries(
  OpenKneeboard-VRKneeboard
  PUBLIC
  _libheaders
  ThirdParty::DirectXTK
  OpenKneeboard-GameEvent
  OpenKneeboard-RayIntersectsRect
  OpenKneeboard-SHM
)

set(VERSION_CPP_FILE "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")
add_custom_target(
  OpenKneeboard-version-sources
  COMMAND
  ${CMAKE_COMMAND}
  -DVERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
  -DVERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
  -DVERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
  -DVERSION_BUILD=${CMAKE_PROJECT_VERSION_TWEAK}
  -DRELEASE_NAME=${RELEASE_NAME}
  "-DINPUT_CPP_FILE=${CMAKE_CURRENT_SOURCE_DIR}/version.cpp.in"
  "-DOUTPUT_CPP_FILE=${VERSION_CPP_FILE}"
  "-P${CMAKE_CURRENT_SOURCE_DIR}/../version.cmake"
  DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/../version.cmake"
  "version.cpp.in"
  BYPRODUCTS
  "${VERSION_CPP_FILE}"
  WORKING_DIRECTORY
  "${CMAKE_SOURCE_DIR}"
)
ok_add_library(OpenKneeboard-version STATIC "${VERSION_CPP_FILE}")
target_include_directories(
  OpenKneeboard-version
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

ok_add_library(OpenKneeboard-shims INTERFACE)
target_include_directories(
  OpenKneeboard-shims
  INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
  OpenKneeboard-shims
  INTERFACE
  OpenKneeboard-dprint
)

ok_add_library(OpenKneeboard-OpenXRMode STATIC OpenXRMode.cpp)
target_link_libraries(
  OpenKneeboard-OpenXRMode
  PUBLIC
  ThirdParty::JSON
  _libheaders
)
target_link_libraries(
  OpenKneeboard-OpenXRMode
  PRIVATE
  OpenKneeboard-RunSubprocessAsync
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-dprint
  ThirdParty::CppWinRT
)

ok_add_library(OpenKneeboard-json STATIC json.cpp)
target_link_libraries(
  OpenKneeboard-json
  PUBLIC
  OpenKneeboard-dprint
  ThirdParty::JSON
  _libheaders
)

ok_add_library(
  OpenKneeboard-SteamVRKneeboard
  STATIC
  SteamVRKneeboard.cpp
)
target_link_libraries(
  OpenKneeboard-SteamVRKneeboard
  PUBLIC
  OpenKneeboard-DXResources
  OpenKneeboard-D3D11
  OpenKneeboard-SHM-Client-D3D11
  OpenKneeboard-VRKneeboard
  OpenKneeboard-config
  ThirdParty::DirectXTK
  ThirdParty::OpenVR
  _libheaders
)
target_link_libraries(
  OpenKneeboard-SteamVRKneeboard
  PRIVATE
  OpenKneeboard-dprint
)

ok_add_library(OpenKneeboard-Filesystem STATIC Filesystem.cpp)
target_link_libraries(
  OpenKneeboard-Filesystem
  PUBLIC
  OpenKneeboard-shims
  OpenKneeboard-config
  _libheaders
)
target_link_libraries(
  OpenKneeboard-Filesystem
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-UTF8
  System::Shlwapi
  ThirdParty::CppWinRT
)

ok_add_library(OpenKneeboard-WindowCaptureControl STATIC WindowCaptureControl.cpp)
target_link_libraries(
  OpenKneeboard-WindowCaptureControl
  PRIVATE
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-dprint
  OpenKneeboard-shims
)
target_link_libraries(
  OpenKneeboard-WindowCaptureControl
  PUBLIC
  OpenKneeboard-handles
  _libheaders
)

ok_add_library(
  OpenKneeboard-RenderDoc
  STATIC
  RenderDoc.cpp
)
target_link_libraries(
  OpenKneeboard-RenderDoc
  PUBLIC
  _libheaders
  PRIVATE
  ThirdParty::RenderDoc
)