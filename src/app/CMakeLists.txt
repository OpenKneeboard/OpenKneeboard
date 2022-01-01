file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")

set(INJECTABLES_CPP "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Injectables.cpp")
file(GENERATE OUTPUT "${INJECTABLES_CPP}" INPUT Injectables.cpp.in)
add_executable(OpenKneeboard WIN32 ${SOURCES} ${INJECTABLES_CPP})

target_link_libraries(
  OpenKneeboard
  wx::core
  wx::base
  detours
  openkneeboard-shm
  openkneeboard-tabs
  dprint
  json
  gameevent
  games
  rendererror)
target_include_directories(OpenKneeboard PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
