add_library(_libheaders INTERFACE)
target_include_directories(_libheaders INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/OpenKneeboard/config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/OpenKneeboard/config.h"
  @ONLY
)
add_library(OpenKneeboard-config INTERFACE)
target_include_directories(
  OpenKneeboard-config
  INTERFACE
  ${CMAKE_CURRENT_BINARY_DIR}/include
)

add_library(OpenKneeboard-GetSystemColor STATIC GetSystemColor.cpp)
target_link_libraries(
  OpenKneeboard-GetSystemColor
  PUBLIC
  _libheaders)

add_library(OpenKneeboard-FilesDiffer STATIC FilesDiffer.cpp)
target_link_libraries(
  OpenKneeboard-FilesDiffer
  PUBLIC
  _libheaders)

add_library(OpenKneeboard-UTF8 STATIC utf8.cpp)
target_link_libraries(
  OpenKneeboard-UTF8
  PRIVATE
  _libheaders
)
target_link_libraries(
  OpenKneeboard-UTF8
  PUBLIC
  ThirdParty::JSON
)

add_library(OpenKneeboard-D3D11 STATIC D3D11.cpp)
target_link_libraries(
  OpenKneeboard-D3D11
  PUBLIC
  _libheaders
)
target_link_libraries(
  OpenKneeboard-D3D11
  PRIVATE
  ThirdParty::DirectXTK
  OpenKneeboard-config
)

add_library(OpenKneeboard-DXResources STATIC DXResources.cpp)
target_link_libraries(OpenKneeboard-DXResources PUBLIC _libheaders)

set(RUNTIME_FILES_CPP "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/RuntimeFiles.cpp")
file(GENERATE OUTPUT "${RUNTIME_FILES_CPP}" INPUT RuntimeFiles.cpp.in)
add_library(
  OpenKneeboard-RuntimeFiles
  STATIC
  "${RUNTIME_FILES_CPP}"
  RuntimeFilesGetDirectory.cpp
  RuntimeFilesInstall.cpp
)
target_link_libraries(OpenKneeboard-RuntimeFiles PUBLIC _libheaders)
target_link_libraries(OpenKneeboard-RuntimeFiles PRIVATE OpenKneeboard-dprint)

add_library(OpenKneeboard-SHM STATIC SHM.cpp)
target_link_libraries(OpenKneeboard-SHM PRIVATE fmt OpenKneeboard-config OpenKneeboard-dprint)
target_link_libraries(OpenKneeboard-SHM PUBLIC _libheaders)

add_library(OpenKneeboard-dprint STATIC dprint.cpp)
target_link_libraries(OpenKneeboard-dprint PUBLIC fmt)
target_link_libraries(OpenKneeboard-dprint PUBLIC _libheaders)

add_library(OpenKneeboard-consolelib STATIC ConsoleLoopCondition.cpp)
target_link_libraries(OpenKneeboard-consolelib PUBLIC _libheaders)

add_library(OpenKneeboard-GameEvent STATIC GameEvent.cpp)
target_link_libraries(OpenKneeboard-GameEvent PRIVATE fmt OpenKneeboard-config OpenKneeboard-dprint)
target_link_libraries(OpenKneeboard-GameEvent PUBLIC _libheaders OpenKneeboard-UTF8)

add_library(OpenKneeboard-D2DErrorRenderer STATIC D2DErrorRenderer.cpp)
target_link_libraries(
  OpenKneeboard-D2DErrorRenderer
  PUBLIC
  D2d1
  OpenKneeboard-UTF8
  Dwrite
  _libheaders)

add_library(OpenKneeboard-RayIntersectsRect STATIC RayIntersectsRect.cpp)
target_link_libraries(
  OpenKneeboard-RayIntersectsRect
  PUBLIC
  _libheaders
  ThirdParty::DirectXTK)
target_link_libraries(
  OpenKneeboard-RayIntersectsRect
  PRIVATE
  OpenKneeboard-dprint)

add_library(OpenKneeboard-scope_guard STATIC scope_guard.cpp)
target_link_libraries(
  OpenKneeboard-scope_guard
  PUBLIC
  _libheaders)

add_library(OpenKneeboard-tablets STATIC WintabTablet.cpp)
target_link_libraries(
  OpenKneeboard-tablets
  PRIVATE
  _libheaders
  wintab
  OpenKneeboard-dprint
  OpenKneeboard-UTF8
)

add_library(OpenKneeboard-VRKneeboard STATIC VRKneeboard.cpp)
target_link_libraries(
  OpenKneeboard-VRKneeboard
  PUBLIC
  _libheaders
  ThirdParty::DirectXTK
  OpenKneeboard-RayIntersectsRect
  OpenKneeboard-SHM
)

set(VERSION_HEADER_FILE "${CMAKE_CURRENT_BINARY_DIR}/include/OpenKneeboard/version.h")
add_custom_target(
  OpenKneeboard-version-sources
  COMMAND
  ${CMAKE_COMMAND}
  -DVERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
  -DVERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
  -DVERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
  "-DINPUT_HEADER_FILE=${CMAKE_CURRENT_SOURCE_DIR}/include/OpenKneeboard/version.h.in"
  "-DOUTPUT_HEADER_FILE=${VERSION_HEADER_FILE}"
  "-P${CMAKE_CURRENT_SOURCE_DIR}/../version.cmake"
  BYPRODUCTS
  "${VERSION_HEADER_FILE}"
  WORKING_DIRECTORY
  "${CMAKE_SOURCE_DIR}"
)
add_library(OpenKneeboard-version INTERFACE)
add_dependencies(OpenKneeboard-version INTERFACE OpenKneeboard-version-sources)
target_include_directories(
  OpenKneeboard-version
  INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}/include"
)

add_library(OpenKneeboard-shims INTERFACE)
target_include_directories(
  OpenKneeboard-shims
  INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_library(OpenKneeboard-OpenXRMode STATIC OpenXRMode.cpp)
target_link_libraries(
  OpenKneeboard-OpenXRMode
  PUBLIC
  ThirdParty::JSON
  _libheaders
)
target_link_libraries(
  OpenKneeboard-OpenXRMode
  PRIVATE
  OpenKneeboard-RuntimeFiles
  ThirdParty::fmt
)
