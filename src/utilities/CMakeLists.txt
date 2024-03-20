include(GNUInstallDirs)

function(add_helper_executable TARGET)
  ok_add_executable(${TARGET} ${ARGN})
  install(TARGETS "${TARGET}")
endfunction()

function(add_utility_executable TARGET)
  ok_add_executable(${TARGET} ${ARGN})
  install(TARGETS "${TARGET}" DESTINATION "utilities")
endfunction()

ok_add_executable(
  test-gameevent-feeder
  test-gameevent-feeder.cpp
  remote-traceprovider.cpp
)
target_link_libraries(
  test-gameevent-feeder
  OpenKneeboard-GameEvent
  OpenKneeboard-consolelib
  OpenKneeboard-games
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
  OpenKneeboard-GameEvent
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
  OpenKneeboard-dprint
  OpenKneeboard-GameEvent
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
  OpenKneeboard-config
  OpenKneeboard-D2DErrorRenderer
  OpenKneeboard-D3D11
  OpenKneeboard-D3D12
  OpenKneeboard-Vulkan
  OpenKneeboard-DXResources
  OpenKneeboard-Filesystem
  OpenKneeboard-GameEvent
  OpenKneeboard-GetSystemColor
  OpenKneeboard-RenderDoc
  OpenKneeboard-SHM
  OpenKneeboard-SHM-Client-D3D11
  OpenKneeboard-SHM-Client-D3D12
  OpenKneeboard-SHM-Client-Vulkan
  OpenKneeboard-Viewer-Shaders
  OpenKneeboard-scope_guard
  System::D2d1
  System::D3d11
  System::Dxgi
  ThirdParty::DirectXTex
  ThirdParty::DirectXTK
  ThirdParty::OutPtr
)

ok_add_executable(
  fake-dcs
  fake-dcs.cpp
  remote-traceprovider.cpp
)
target_link_libraries(fake-dcs OpenKneeboard-games OpenKneeboard-GameEvent)

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
  RELOAD_CURRENT_TAB
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
    OpenKneeboard-GameEvent
    OpenKneeboard-dprint
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

# Calling this the 'installation helper' instead of the
# 'repair helper' to give a more relatable UAC prompt.
#
# This exists to work around Microsoft not following their own
# versioning advice, and downgrading DLL versions in Windows
# App SDK updates.
add_helper_executable(
  OpenKneeboard-Installation-Helper
  WIN32
  repair-helper.cpp
  icon.rc
)
target_link_libraries(
  OpenKneeboard-Installation-Helper
  PRIVATE
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-config
  OpenKneeboard-dprint
  System::Msi
)

add_helper_executable(
  OpenKneeboard-Set-Desired-Elevation-Helper
  WIN32
  set-desired-elevation-helper.cpp
  icon.rc
)
target_link_libraries(
  OpenKneeboard-Set-Desired-Elevation-Helper
  PRIVATE
  OpenKneeboard-Elevation
  OpenKneeboard-dprint
)

add_helper_executable(
  OpenKneeboard-WindowCaptureHook-Helper
  WIN32
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
