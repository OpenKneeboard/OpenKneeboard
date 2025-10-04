include(GNUInstallDirs)

function(add_helper_executable TARGET)
  set(options 32BIT_ONLY)
  set(valueArgs)
  set(multiValueArgs)
  cmake_parse_arguments(HELPER_ARG "${options}" "${valueArgs}" "${multiValueArgs}" ${ARGN})

  ok_add_executable(${TARGET} EXCLUDE_FROM_ALL RUNTIME_DEPENDENCY ${ARGN})
  if(HELPER_ARG_32BIT_ONLY AND NOT BUILD_IS_32BIT)
    return()
  endif()
endfunction()

function(add_utility_executable TARGET)
  ok_add_executable(
    ${TARGET}
    RUNTIME_EXE
    OUTPUT_SUBDIR "utilities"
    ${ARGN}
  )
endfunction()

ok_add_executable(
  test-apievent-feeder
  test-apievent-feeder.cpp
  remote-traceprovider.cpp
)
target_link_libraries(
  test-apievent-feeder
  PRIVATE
  OpenKneeboard-APIEvent
  OpenKneeboard-consolelib
  OpenKneeboard-games
  OpenKneeboard-tracing
)

add_utility_executable(
  OpenKneeboard-Plugin-Tab-Action
  WIN32
  plugin-tab-action.cpp
)
target_link_libraries(
  OpenKneeboard-Plugin-Tab-Action
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-APIEvent
  OpenKneeboard-tracing
)

add_utility_executable(
  OpenKneeboard-RemoteControl-SET_TAB
  WIN32
  set-tab-remote.cpp
  remote-traceprovider.cpp
)
target_link_libraries(
  OpenKneeboard-RemoteControl-SET_TAB
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-APIEvent
  OpenKneeboard-tracing
)

add_utility_executable(
  OpenKneeboard-RemoteControl-SET_PROFILE
  WIN32
  set-profile-remote.cpp
  remote-traceprovider.cpp
)
target_link_libraries(
  OpenKneeboard-RemoteControl-SET_PROFILE
  PRIVATE
  OpenKneeboard-APIEvent
  OpenKneeboard-dprint
  OpenKneeboard-tracing
)

add_utility_executable(
  OpenKneeboard-Viewer
  WIN32
  viewer.cpp
  viewer-settings.cpp
  viewer-d3d11.cpp
  viewer-d3d12.cpp
  viewer-vulkan.cpp
  icon.rc
)
target_link_libraries(
  OpenKneeboard-Viewer
  PRIVATE
  OpenKneeboard-fatal
  OpenKneeboard-config
  OpenKneeboard-D2DErrorRenderer
  OpenKneeboard-D3D11
  OpenKneeboard-D3D12
  OpenKneeboard-Vulkan
  OpenKneeboard-DXResources
  OpenKneeboard-Filesystem
  OpenKneeboard-APIEvent
  OpenKneeboard-GetSystemColor
  OpenKneeboard-RenderDoc
  OpenKneeboard-SHM
  OpenKneeboard-SHM-Client-D3D11
  OpenKneeboard-SHM-Client-D3D12
  OpenKneeboard-SHM-Client-Vulkan
  OpenKneeboard-Viewer-Shaders
  OpenKneeboard-scope_exit
  System::D2d1
  System::D3d11
  System::Dcomp
  System::Dxgi
  ThirdParty::DirectXTex
  ThirdParty::DirectXTK
)

ok_add_executable(
  fake-dcs
  fake-dcs.cpp
  remote-traceprovider.cpp
)
target_link_libraries(
  fake-dcs
  PRIVATE
  OpenKneeboard-APIEvent
  OpenKneeboard-games
  OpenKneeboard-tracing
)

# Mostly to workaround Huion driver limitations, but maybe also useful for
# StreamDeck and VoiceAttack
#
# Remember to update the docs when you add stuff here :)
set(
  SIMPLE_REMOTE_ACTIONS
  CYCLE_ACTIVE_VIEW
  DECREASE_BRIGHTNESS
  DISABLE_TINT
  ENABLE_TINT
  HIDE
  INCREASE_BRIGHTNESS
  NEXT_BOOKMARK
  NEXT_PAGE
  NEXT_PROFILE
  NEXT_TAB
  PREVIOUS_BOOKMARK
  PREVIOUS_PAGE
  PREVIOUS_PROFILE
  PREVIOUS_TAB
  RECENTER_VR
  REPAINT_NOW
  SHOW
  SWAP_FIRST_TWO_VIEWS
  TOGGLE_BOOKMARK
  TOGGLE_FORCE_ZOOM
  TOGGLE_TINT
  TOGGLE_VISIBILITY
)

foreach(REMOTE_ACTION ${SIMPLE_REMOTE_ACTIONS})
  set(TARGET "OpenKneeboard-RemoteControl-${REMOTE_ACTION}")
  add_utility_executable(
    "${TARGET}"
    WIN32
    simple-remote.cpp
    remote-traceprovider.cpp
  )
  target_compile_definitions(
    "${TARGET}"
    PRIVATE
    "REMOTE_ACTION=${REMOTE_ACTION}"
  )
  target_link_libraries(
    "${TARGET}"
    PRIVATE
    OpenKneeboard-APIEvent
    OpenKneeboard-dprint
    OpenKneeboard-tracing
  )
endforeach()

add_helper_executable(
  OpenKneeboard-OpenXR-Helper
  WIN32
  openxr-helper.cpp
  icon.rc
)
target_link_libraries(
  OpenKneeboard-OpenXR-Helper
  PRIVATE
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-dprint
)

add_helper_executable(
  OpenKneeboard-WindowCaptureHook-Helper
  WIN32
  32BIT_ONLY
  WindowCaptureHook-helper.cpp
)
target_link_libraries(
  OpenKneeboard-WindowCaptureHook-Helper
  PRIVATE
  OpenKneeboard-WindowCaptureControl
  OpenKneeboard-consolelib
  OpenKneeboard-dprint
  OpenKneeboard-shims
  System::Shell32
)

add_executable(capi-test capi-test.cpp)
target_include_directories(
  capi-test
  PRIVATE
  "$<TARGET_PROPERTY:OpenKneeboard-c-api,INTERFACE_INCLUDE_DIRECTORIES>")
set_property(
  TARGET capi-test
  PROPERTY CXX_STANDARD 17)

ok_add_executable(symbolize-traces symbolize-traces.cpp)
target_link_libraries(
  symbolize-traces
  PRIVATE
  OpenKneeboard-shims
  ThirdParty::DIA
  ThirdParty::WIL
)
set_target_properties(
  symbolize-traces
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${BUILD_OUT_PDBDIR}"
)
add_custom_command(
  TARGET symbolize-traces
  POST_BUILD
  COMMAND
  "${CMAKE_COMMAND}" -E copy_if_different
  "${DIASDK_DLL}"
  "$<TARGET_FILE_DIR:symbolize-traces>/"
)

if (BUILD_IS_64BIT)
  set(TARGET_NAME "OpenKneeboard-Chromium")
  set(MANIFEST_PATH "${CMAKE_CURRENT_BINARY_DIR}/OpenKneeboard-Chromium.manifest")

  configure_file(
    "utility.manifest.in"
    "${MANIFEST_PATH}"
    @ONLY
    NEWLINE_STYLE UNIX
  )

  add_helper_executable(
    OpenKneeboard-Chromium
    WIN32
    chromium-worker.cpp
    "${MANIFEST_PATH}"
  )
  target_link_libraries(
    OpenKneeboard-Chromium
    PRIVATE
    Cef::LibCef
    OpenKneeboard-Filesystem
    OpenKneeboard-fatal
  )
  set_target_properties(
    OpenKneeboard-Chromium
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BUILD_OUT_ROOT}/libexec/cef"
  )
endif()
