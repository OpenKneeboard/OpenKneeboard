file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")

add_library(openkneeboard-tabs STATIC "${SOURCES}")
target_include_directories(
  openkneeboard-tabs
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(
  openkneeboard-tabs
  PUBLIC
  wx::core
  wx::base
  games
  gameevent)
