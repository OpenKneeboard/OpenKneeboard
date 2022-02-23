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
)

add_executable(test-viewer WIN32 test-viewer.cpp)
target_link_libraries(
  test-viewer
  OpenKneeboard-D2DErrorRenderer
  OpenKneeboard-DXResources
  OpenKneeboard-GetSystemColor
  OpenKneeboard-SHM
  OpenKneeboard-scope_guard
)

add_executable(fake-dcs fake-dcs.cpp)
target_link_libraries(fake-dcs fmt OpenKneeboard-games OpenKneeboard-GameEvent)

add_executable(OpenKneeboard-Launch-WindowsApp WIN32 launch-msix.cpp icon.rc)
sign_target(OpenKneeboard-Launch-WindowsApp)
install(TARGETS OpenKneeboard-Launch-WindowsApp COMPONENT RemoteControl)
install(FILES "$<TARGET_PDB_FILE:OpenKneeboard-Launch-WindowsApp>" TYPE BIN COMPONENT DebugSymbols)

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
  add_executable("${TARGET}" WIN32 simple-remote.cpp)
  target_compile_definitions(
    "${TARGET}"
    PUBLIC
    "REMOTE_ACTION=${REMOTE_ACTION}"
  )
  target_link_libraries("${TARGET}" OpenKneeboard-GameEvent)
  sign_target("${TARGET}")
  install(TARGETS "${TARGET}" COMPONENT RemoteControl)
  install(FILES "$<TARGET_PDB_FILE:${TARGET}>" TYPE BIN COMPONENT DebugSymbols)
endforeach()
