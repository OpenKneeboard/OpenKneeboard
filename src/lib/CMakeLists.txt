ok_add_library(OpenKneeboard-Lib-Headers INTERFACE)
target_include_directories(OpenKneeboard-Lib-Headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")


configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/OpenKneeboard/config.in.hpp"
  "${CMAKE_CURRENT_BINARY_DIR}/include/OpenKneeboard/config.hpp"
  @ONLY
)
file(
  GENERATE
  OUTPUT
  "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/include/OpenKneeboard/detail/config.hpp"
  CONTENT "\
namespace OpenKneeboard::detail::Config {
  constexpr auto BuildType = \"$<CONFIG>\";
}"
)
ok_add_library(OpenKneeboard-config INTERFACE)
target_include_directories(
  OpenKneeboard-config
  INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/include"
)
target_link_libraries(OpenKneeboard-config INTERFACE OpenKneeboard-Lib-Headers)

include(Geometry2D.cmake)

ok_add_library(OpenKneeboard-ThreadGuard STATIC ThreadGuard.cpp)
target_link_libraries(OpenKneeboard-ThreadGuard PUBLIC OpenKneeboard-Lib-Headers)
target_link_libraries(OpenKneeboard-ThreadGuard PRIVATE OpenKneeboard-dprint)

ok_add_library(
  OpenKneeboard-fatal
  STATIC
  fatal.cpp
  HEADERS
  include/OpenKneeboard/fatal.hpp
  INCLUDE_DIRECTORIES
  include
)
target_link_libraries(
  OpenKneeboard-fatal
  PUBLIC
  OpenKneeboard-config
  PRIVATE
  OpenKneeboard-Elevation
  OpenKneeboard-dprint
  OpenKneeboard-shims
  OpenKneeboard-Filesystem
  OpenKneeboard-version
  ThirdParty::CppWinRT
  ThirdParty::WIL
  ThirdParty::detours
  ThirdParty::felly
)

ok_add_library(OpenKneeboard-GetMainHWND STATIC GetMainHWND.cpp)
target_link_libraries(
  OpenKneeboard-GetMainHWND
  PUBLIC
  OpenKneeboard-Lib-Headers
  OpenKneeboard-win32
)
target_link_libraries(
  OpenKneeboard-GetMainHWND
  PRIVATE
  OpenKneeboard-config
  OpenKneeboard-shims
)

ok_add_library(OpenKneeboard-GetSystemColor STATIC GetSystemColor.cpp)
target_link_libraries(
  OpenKneeboard-GetSystemColor
  PUBLIC
  OpenKneeboard-Lib-Headers)

ok_add_library(OpenKneeboard-handles INTERFACE)
target_link_libraries(
  OpenKneeboard-handles
  INTERFACE
  OpenKneeboard-Lib-Headers
)

ok_add_library(OpenKneeboard-Elevation STATIC Elevation.cpp)
target_link_libraries(
  OpenKneeboard-Elevation
  PUBLIC
  OpenKneeboard-Lib-Headers
)
target_link_libraries(
  OpenKneeboard-Elevation
  PRIVATE
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-scope_exit
  ThirdParty::CppWinRT
  System::Shell32
)

ok_add_library(OpenKneeboard-UTF8 STATIC utf8.cpp)
target_link_libraries(
  OpenKneeboard-UTF8
  PUBLIC
  ThirdParty::JSON
  OpenKneeboard-Lib-Headers
  PRIVATE
  OpenKneeboard-fatal
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
  OpenKneeboard-Lib-Headers
)

ok_add_library(OpenKneeboard-tracing STATIC tracing.cpp)
target_link_libraries(
  OpenKneeboard-tracing
  PRIVATE
  OpenKneeboard-dprint
  ThirdParty::CppWinRT
)
target_link_libraries(
  OpenKneeboard-tracing
  PUBLIC
  OpenKneeboard-Lib-Headers
)

ok_add_library(
  OpenKneeboard-D3D11
  STATIC
  D3D11.cpp
  D3D11/SpriteBatch.cpp
)
target_link_libraries(
  OpenKneeboard-D3D11
  PUBLIC
  System::D3d11
  OpenKneeboard-shims
  ThirdParty::DirectXTK
  ThirdParty::CppWinRT
  OpenKneeboard-Lib-Headers
  OpenKneeboard-Sprite-DXBC
  PRIVATE
  OpenKneeboard-dprint
  System::Dxguid
)

ok_add_library(
  OpenKneeboard-D3D12
  STATIC
  D3D12/SpriteBatch.cpp
)
target_link_libraries(
  OpenKneeboard-D3D12
  PUBLIC
  System::D3d12
  OpenKneeboard-shims
  ThirdParty::DirectXTK12
  ThirdParty::CppWinRT
  OpenKneeboard-Lib-Headers
  OpenKneeboard-SpriteBatch-DXIL
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
  OpenKneeboard-Lib-Headers
  OpenKneeboard-SpriteBatch-SPIRV
)

ok_add_library(OpenKneeboard-DXResources STATIC DXResources.cpp)
target_link_libraries(
  OpenKneeboard-DXResources
  PUBLIC
  OpenKneeboard-Lib-Headers
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
target_link_libraries(
  OpenKneeboard-RuntimeFiles
  PUBLIC
  OpenKneeboard-Lib-Headers
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-Filesystem
)
if (ENABLE_APP)
  set(RUNTIME_FILES_APP_CPP "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/RuntimeFiles-app.cpp")
  file(GENERATE OUTPUT "${RUNTIME_FILES_APP_CPP}" INPUT RuntimeFiles-app.cpp.in)
  target_sources(OpenKneeboard-RuntimeFiles PRIVATE "${RUNTIME_FILES_APP_CPP}")
endif ()

ok_add_library(
  OpenKneeboard-SHM
  STATIC
  SHM.cpp
  SHM/ActiveConsumers.cpp
)
target_link_libraries(
  OpenKneeboard-SHM
  PUBLIC
  OpenKneeboard-dprint
  OpenKneeboard-Lib-Headers
  OpenKneeboard-StateMachine
  PRIVATE
  OpenKneeboard-Elevation
  OpenKneeboard-config
  OpenKneeboard-fatal
  OpenKneeboard-shims
  OpenKneeboard-version
  OpenKneeboard-win32
  Boost::container
)

ok_add_library(
  OpenKneeboard-SHM-Client-D3D11
  STATIC
  SHM/D3D11.cpp
  D3D11/Renderer.cpp
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
  D3D12/Renderer.cpp
)
target_link_libraries(
  OpenKneeboard-SHM-Client-D3D12
  PUBLIC
  OpenKneeboard-shims
  OpenKneeboard-SHM
  OpenKneeboard-D3D12
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
  OpenKneeboard-SHM
  OpenKneeboard-Vulkan
  OpenKneeboard-shims
  ThirdParty::CppWinRT
  ThirdParty::VulkanHeaders
)

ok_add_library(
  OpenKneeboard-dprint
  STATIC
  dprint.cpp
  HEADERS
  include/OpenKneeboard/dprint.hpp
  INCLUDE_DIRECTORIES
  include
)

target_link_libraries(
  OpenKneeboard-dprint
  PUBLIC
  OpenKneeboard-config
  OpenKneeboard-fatal
  OpenKneeboard-scope_exit
  ThirdParty::bindline
)
target_link_libraries(OpenKneeboard-dprint
  PRIVATE
  OpenKneeboard-scope_exit
)

ok_add_library(OpenKneeboard-consolelib STATIC ConsoleLoopCondition.cpp)
target_link_libraries(
  OpenKneeboard-consolelib
  PUBLIC
  OpenKneeboard-Lib-Headers
  PRIVATE
  OpenKneeboard-fatal
  OpenKneeboard-win32
)

ok_add_library(OpenKneeboard-APIEvent STATIC APIEvent.cpp)
target_link_libraries(OpenKneeboard-APIEvent
  PUBLIC
  OpenKneeboard-Lib-Headers
  OpenKneeboard-UTF8
  OpenKneeboard-json
  PRIVATE
  OpenKneeboard-config
  OpenKneeboard-dprint
)

ok_add_library(OpenKneeboard-D2DErrorRenderer STATIC D2DErrorRenderer.cpp)
target_link_libraries(
  OpenKneeboard-D2DErrorRenderer
  PUBLIC
  OpenKneeboard-DXResources
  OpenKneeboard-UTF8
  OpenKneeboard-config
  System::D2d1
  System::Dwrite
  OpenKneeboard-Lib-Headers)

ok_add_library(OpenKneeboard-RayIntersectsRect STATIC RayIntersectsRect.cpp)
target_link_libraries(
  OpenKneeboard-RayIntersectsRect
  PUBLIC
  OpenKneeboard-Lib-Headers
  ThirdParty::DirectXTK)
target_link_libraries(
  OpenKneeboard-RayIntersectsRect
  PRIVATE
  OpenKneeboard-dprint)

ok_add_library(OpenKneeboard-StateMachine INTERFACE)
target_link_libraries(
  OpenKneeboard-StateMachine
  INTERFACE
  OpenKneeboard-Lib-Headers
  OpenKneeboard-dprint
  ThirdParty::magic_enum
)

ok_add_library(OpenKneeboard-scope_exit INTERFACE)
target_link_libraries(
  OpenKneeboard-scope_exit
  INTERFACE
  OpenKneeboard-Lib-Headers
  ThirdParty::felly
)

ok_add_library(OpenKneeboard-VRKneeboard STATIC VRKneeboard.cpp)
target_link_libraries(
  OpenKneeboard-VRKneeboard
  PUBLIC
  OpenKneeboard-Lib-Headers
  ThirdParty::DirectXTK
  OpenKneeboard-RayIntersectsRect
  OpenKneeboard-SHM
)

set(VERSION_CPP_FILE "${CMAKE_CURRENT_BINARY_DIR}/include/OpenKneeboard/version.hpp")
add_custom_target(
  OpenKneeboard-version-sources
  COMMAND
  ${CMAKE_COMMAND}
  -DVERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
  -DVERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
  -DVERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
  -DVERSION_BUILD=${CMAKE_PROJECT_VERSION_TWEAK}
  -DRELEASE_NAME=${RELEASE_NAME}
  -DOPENXR_API_LAYER_IMPLEMENTATION_VERSION=${OPENXR_API_LAYER_IMPLEMENTATION_VERSION}
  "-DINPUT_CPP_FILE=${CMAKE_CURRENT_SOURCE_DIR}/include/OpenKneeboard/version.in.hpp"
  "-DOUTPUT_CPP_FILE=${VERSION_CPP_FILE}"
  "-P${CMAKE_CURRENT_SOURCE_DIR}/../version.cmake"
  DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/../version.cmake"
  "version.hpp.in"
  BYPRODUCTS
  "${VERSION_CPP_FILE}"
  WORKING_DIRECTORY
  "${CMAKE_SOURCE_DIR}"
)
ok_add_library(OpenKneeboard-version INTERFACE)
target_include_directories(
  OpenKneeboard-version
  INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}/include"
)
add_dependencies(OpenKneeboard-version OpenKneeboard-version-sources)
ok_add_library(
  OpenKneeboard-semver
  STATIC
  semver.cpp
  HEADERS
  include/OpenKneeboard/semver.hpp
  INCLUDE_DIRECTORIES
  include
)
target_link_libraries(
  OpenKneeboard-semver
  PRIVATE
  OpenKneeboard-dprint
  ThirdParty::SemVer
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
  OpenKneeboard-fatal
)

ok_add_library(OpenKneeboard-json STATIC json.cpp)
target_link_libraries(
  OpenKneeboard-json
  PUBLIC
  OpenKneeboard-dprint
  ThirdParty::JSON
  OpenKneeboard-Lib-Headers
)

ok_add_library(OpenKneeboard-Filesystem STATIC Filesystem.cpp)
target_link_libraries(
  OpenKneeboard-Filesystem
  PUBLIC
  OpenKneeboard-shims
  OpenKneeboard-config
  OpenKneeboard-Lib-Headers
  PRIVATE
  OpenKneeboard-StateMachine
  OpenKneeboard-UTF8
  OpenKneeboard-dprint
  OpenKneeboard-fatal
  System::Shlwapi
  ThirdParty::CppWinRT
  ThirdParty::WIL
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
  OpenKneeboard-Lib-Headers
)

ok_add_library(
  OpenKneeboard-RenderDoc
  STATIC
  RenderDoc.cpp
)
target_link_libraries(
  OpenKneeboard-RenderDoc
  PUBLIC
  OpenKneeboard-Lib-Headers
  PRIVATE
  OpenKneeboard-fatal
  ThirdParty::RenderDoc
)
ok_add_library(
  OpenKneeboard-EnumerateProcesses
  STATIC
  EnumerateProcesses.cpp
  HEADERS
  include/OpenKneeboard/EnumerateProcesses.hpp
  INCLUDE_DIRECTORIES
  include
)
target_link_libraries(
  OpenKneeboard-EnumerateProcesses
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-tracing
)

ok_add_library(
  OpenKneeboard-win32
  INTERFACE
  HEADERS
  include/OpenKneeboard/Win32.hpp
  INCLUDE_DIRECTORIES
  include
)
target_link_libraries(
  OpenKneeboard-win32
  INTERFACE
  OpenKneeboard-fatal
  OpenKneeboard-shims
  ThirdParty::bindline
  ThirdParty::CppWinRT
)

add_library(OpenKneeboard-task INTERFACE)
target_link_libraries(
  OpenKneeboard-task
  INTERFACE
  OpenKneeboard-StateMachine
  OpenKneeboard-dprint
  OpenKneeboard-fatal
  OpenKneeboard-shims
  ThirdParty::CppWinRT
  ThirdParty::magic_enum
)

ok_add_library(
  OpenKneeboard-UserAction
  INTERFACE
  HEADERS include/OpenKneeboard/UserAction.hpp
  INCLUDE_DIRECTORIES include
)