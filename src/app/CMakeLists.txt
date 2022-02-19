file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(OpenKneeboard WIN32 ${SOURCES} OpenKneeboard.rc)
target_compile_definitions(
  OpenKneeboard
  PRIVATE
  "DIRECTINPUT_VERSION=0x0800"
)
sign_target(OpenKneeboard)

target_link_libraries(
  OpenKneeboard
  PRIVATE
  D2d1
  directxtk
  json
  OpenKneeboard-D2DErrorRenderer
  OpenKneeboard-DXResources
  OpenKneeboard-GameEvent
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-SHM
  OpenKneeboard-UTF8
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-games
  OpenKneeboard-tablets
  OpenKneeboard-version
  openvr
  qpdf
  wx::base
  wx::core
)

set(SOURCE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(SOURCE ${SOURCES})
  get_filename_component(SOURCE_DIR ${SOURCE} DIRECTORY)
  list(APPEND SOURCE_DIRS "${SOURCE_DIR}")
endforeach()
list(REMOVE_DUPLICATES SOURCE_DIRS)
target_include_directories(
  OpenKneeboard
  PRIVATE
  ${SOURCE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/UserInput/include
  ${CMAKE_CURRENT_SOURCE_DIR}/GamesList/include
  ${CMAKE_CURRENT_SOURCE_DIR}/TabsList/include
  ${CMAKE_CURRENT_SOURCE_DIR}/tabs/include
)

install(TARGETS OpenKneeboard)
install(FILES "$<TARGET_PDB_FILE:OpenKneeboard>" TYPE BIN COMPONENT DebugSymbols)

set(
  RUNTIME_FILES
  "$<TARGET_RUNTIME_DLLS:OpenKneeboard>"
  "${CMAKE_SOURCE_DIR}/src/dcs-hook/OpenKneeboardDCSExt.lua"
)
install(FILES "${RUNTIME_FILES}" TYPE BIN)

set(
  DLL_TARGETS
  OpenKneeboard-Injection-Bootstrapper
  OpenKneeboard-AutoInject-Marker
  OpenKneeboard-TabletProxy
  OpenKneeboard-nonvr-d3d11
  OpenKneeboard-oculus-d3d11
  OpenKneeboard-oculus-d3d12
  OpenKneeboard-dcs-ext-dll
)
install(TARGETS ${DLL_TARGETS} LIBRARY DESTINATION bin)

foreach(TARGET ${DLL_TARGETS})
  list(APPEND RUNTIME_FILES "$<TARGET_FILE:${TARGET}>")
  install(FILES "$<TARGET_PDB_FILE:${TARGET}>" TYPE BIN COMPONENT DebugSymbols)
endforeach()

add_custom_target(
  CopyRuntimeFiles
  ALL
  COMMAND
  ${CMAKE_COMMAND} -E copy_if_different
  "${RUNTIME_FILES}"
  "$<TARGET_FILE_DIR:OpenKneeboard>"
  &&
  ${CMAKE_COMMAND} -E touch RuntimeFiles.stamp
  COMMAND_EXPAND_LISTS)

include(InstallRequiredSystemLibraries)

add_custom_target(
  AppxManifest
  ALL
  COMMAND
  ${CMAKE_COMMAND}
  -DVERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
  -DVERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
  -DVERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
  "-DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/appxmanifest.xml.in"
  "-DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/appxmanifest.xml"
  "-P${CMAKE_CURRENT_SOURCE_DIR}/appxmanifest.xml.cmake"
  BYPRODUCTS
  "${CMAKE_CURRENT_BINARY_DIR}/appxmanifest.xml"
  WORKING_DIRECTORY
  "${CMAKE_SOURCE_DIR}"
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/appxmanifest.xml" DESTINATION .)
