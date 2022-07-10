include(GNUInstallDirs)

function(add_helper_executable TARGET)
  add_executable(${TARGET} ${ARGN})
  add_version_rc(${TARGET} ${TARGET}.exe)
  sign_target(${TARGET})
  install(TARGETS "${TARGET}")
  install(FILES "$<TARGET_PDB_FILE:${TARGET}>" TYPE BIN COMPONENT DebugSymbols)
endfunction()

function(add_remote_control_executable TARGET)
  add_executable(${TARGET} ${ARGN})
  add_version_rc(${TARGET} ${TARGET}.exe)
  sign_target(${TARGET})
  install(TARGETS "${TARGET}" COMPONENT RemoteControl)
  install(FILES "$<TARGET_PDB_FILE:${TARGET}>" TYPE BIN COMPONENT DebugSymbols)
endfunction()

add_executable(test-gameevent-feeder test-gameevent-feeder.cpp)
target_link_libraries(
  test-gameevent-feeder
  OpenKneeboard-GameEvent
  OpenKneeboard-consolelib
  OpenKneeboard-games
)

add_executable(test-feeder test-feeder.cpp)
target_link_libraries(
  test-feeder
  OpenKneeboard-config
  OpenKneeboard-consolelib
  OpenKneeboard-dprint
  OpenKneeboard-OpenVRKneeboard
  OpenKneeboard-SHM
  ThirdParty::DirectXTK
  System::D2d1
  System::D3d11
  System::Dwrite
)

add_custom_command(
  TARGET test-feeder
  POST_BUILD
  COMMAND
  "${CMAKE_COMMAND}" -E copy_if_different
  "$<TARGET_PROPERTY:ThirdParty::OpenVR,IMPORTED_LOCATION>"
  "$<TARGET_FILE_DIR:test-feeder>/openvr_api.dll"
)

add_remote_control_executable(OpenKneeboard-Viewer WIN32 test-viewer.cpp)
target_link_libraries(
  OpenKneeboard-Viewer
  PRIVATE
  OpenKneeboard-config
  OpenKneeboard-D2DErrorRenderer
  OpenKneeboard-DXResources
  OpenKneeboard-GameEvent
  OpenKneeboard-GetSystemColor
  OpenKneeboard-SHM
  OpenKneeboard-scope_guard
  System::D2d1
  System::D3d11
  System::Dxgi
)

add_executable(fake-dcs fake-dcs.cpp)
target_link_libraries(fake-dcs OpenKneeboard-games OpenKneeboard-GameEvent)

add_remote_control_executable(OpenKneeboard-Launch-WindowsApp WIN32 launch-msix.cpp icon.rc)

# Mostly to workaround Huion driver limitations, but maybe also useful for
# StreamDeck
set(
  SIMPLE_REMOTE_ACTIONS
  PREVIOUS_TAB
  NEXT_TAB
  PREVIOUS_PAGE
  NEXT_PAGE
  TOGGLE_VISIBILITY
  TOGGLE_FORCE_ZOOM
  SWITCH_KNEEBOARDS
  RECENTER_VR
)
foreach(REMOTE_ACTION ${SIMPLE_REMOTE_ACTIONS})
  set(TARGET "OpenKneeboard-RemoteControl-${REMOTE_ACTION}")
  add_remote_control_executable("${TARGET}" WIN32 simple-remote.cpp)
  target_compile_definitions(
    "${TARGET}"
    PUBLIC
    "REMOTE_ACTION=${REMOTE_ACTION}"
  )
  target_link_libraries("${TARGET}" OpenKneeboard-GameEvent)
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

add_executable(OpenKneeboard-Uninstall-Helper WIN32 uninstall-helper.cpp icon.rc)
add_version_rc(OpenKneeboard-Uninstall-Helper OpenKneeboard-Uninstall-Helper.exe)
sign_target(OpenKneeboard-Uninstall-Helper)
target_link_libraries(
  OpenKneeboard-Uninstall-Helper
  PRIVATE
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-UTF8
  ThirdParty::CppWinRT)
target_link_options(OpenKneeboard-Uninstall-Helper PRIVATE "/MANIFESTUAC:level='requireAdministrator'")
install(
  TARGETS
  OpenKneeboard-Uninstall-Helper
  DESTINATION
  "${CMAKE_INSTALL_BINDIR}/installation")
install(
  TARGETS
  OpenKneeboard-Uninstall-Helper
  COMPONENT
  RemoteControl
  EXCLUDE_FROM_ALL)
install(FILES "$<TARGET_PDB_FILE:OpenKneeboard-Uninstall-Helper>" TYPE BIN COMPONENT DebugSymbols)
