file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(OpenKneeboard WIN32 ${SOURCES})
target_link_libraries(
  OpenKneeboard
  wx::core
  wx::base
  openkneeboard-shm
  openkneeboard-tabs
  dprint
  games)
