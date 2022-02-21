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


add_library(OpenKneeboard-UTF8 STATIC utf8.cpp)
target_link_libraries(
	OpenKneeboard-UTF8
	PRIVATE
	_libheaders
	cppwinrt)

add_library(OpenKneeboard-DXResources STATIC DXResources.cpp)
target_link_libraries(OpenKneeboard-DXResources PUBLIC _libheaders)

set(RUNTIME_FILES_CPP "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/RuntimeFiles.cpp")
file(GENERATE OUTPUT "${RUNTIME_FILES_CPP}" INPUT RuntimeFiles.cpp.in)
add_library(OpenKneeboard-RuntimeFiles STATIC "${RUNTIME_FILES_CPP}")
target_link_libraries(OpenKneeboard-RuntimeFiles PUBLIC _libheaders)

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

add_custom_target(
  VersionHeader
  COMMAND
  ${CMAKE_COMMAND}
  -DVERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
  -DVERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
  -DVERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
  "-DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/include/OpenKneeboard/version.h.in"
  "-DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/include/OpenKneeboard/version.h"
  "-P${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake"
  BYPRODUCTS
  "${CMAKE_CURRENT_BINARY_DIR}/include/OpenKneeboard/version.h"
  WORKING_DIRECTORY
  "${CMAKE_SOURCE_DIR}"
)
add_library(OpenKneeboard-version INTERFACE)
add_dependencies(OpenKneeboard-version INTERFACE VersionHeader)
target_include_directories(
  OpenKneeboard-version
  INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}/include"
)
