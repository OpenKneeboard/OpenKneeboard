file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.cpp")

set(INJECTABLES_CPP "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Injectables.cpp")
file(GENERATE OUTPUT "${INJECTABLES_CPP}" INPUT Injectables.cpp.in)
add_executable(OpenKneeboard WIN32 ${SOURCES} ${INJECTABLES_CPP})
target_compile_definitions(
  OpenKneeboard
  PRIVATE
  "DIRECTINPUT_VERSION=0x0800"
)

target_link_libraries(
  OpenKneeboard
  wx::core
  wx::base
  detours
  openkneeboard-shm
  dprint
  json
  gameevent
  games
  rendererror)

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
  ${CMAKE_CURRENT_SOURCE_DIR}/tabs/include)
  