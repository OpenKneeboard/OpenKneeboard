function(add_released_utility TARGET)
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
  fmt
)

add_executable(test-feeder test-feeder.cpp)
target_link_libraries(
  test-feeder
  OpenKneeboard-config
  OpenKneeboard-consolelib
  OpenKneeboard-dprint
  OpenKneeboard-SHM
  ThirdParty::DirectXTK
)

add_released_utility(OpenKneeboard-Viewer WIN32 test-viewer.cpp)
target_link_libraries(
  OpenKneeboard-Viewer
  OpenKneeboard-D2DErrorRenderer
  OpenKneeboard-DXResources
  OpenKneeboard-GetSystemColor
  OpenKneeboard-SHM
  OpenKneeboard-scope_guard
  ThirdParty::fmt
)

add_executable(fake-dcs fake-dcs.cpp)
target_link_libraries(fake-dcs fmt OpenKneeboard-games OpenKneeboard-GameEvent)

add_released_utility(OpenKneeboard-Launch-WindowsApp WIN32 launch-msix.cpp icon.rc)

# Mostly to workaround Huion driver limitations, but maybe also useful for
# StreamDeck
set(
  SIMPLE_REMOTE_ACTIONS
  TOGGLE_VISIBILITY
  PREVIOUS_TAB
  NEXT_TAB
  PREVIOUS_PAGE
  NEXT_PAGE
)
foreach(REMOTE_ACTION ${SIMPLE_REMOTE_ACTIONS})
  set(TARGET "OpenKneeboard-RemoteControl-${REMOTE_ACTION}")
  add_released_utility("${TARGET}" WIN32 simple-remote.cpp)
  target_compile_definitions(
    "${TARGET}"
    PUBLIC
    "REMOTE_ACTION=${REMOTE_ACTION}"
  )
  target_link_libraries("${TARGET}" OpenKneeboard-GameEvent)
endforeach()

add_released_utility(OpenKneeboard-OpenXR-Helper WIN32 openxr-helper.cpp)
target_link_libraries(
  OpenKneeboard-OpenXR-Helper 
  PRIVATE
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-dprint
)
install(TARGETS OpenKneeboard-OpenXR-Helper COMPONENT Helpers)
install(
  FILES
  "$<TARGET_PDB_FILE:OpenKneeboard-OpenXR-Helper>"
  TYPE BIN
  COMPONENT Helpers-DebugSymbols
)
