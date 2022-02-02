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
  OpenKneeboard-SHM
  OpenKneeboard-D2DErrorRenderer
  wx::core
  wx::base
)

add_executable(fake-dcs fake-dcs.cpp)
target_link_libraries(fake-dcs fmt games OpenKneeboard-GameEvent)
