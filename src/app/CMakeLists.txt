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
  detours
  dprint
  eigen3
  gameevent
  games
  json
  openkneeboard-shm
  openvr
  rendererror
  wx::base
  wx::core)

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

add_custom_command(
  TARGET OpenKneeboard POST_BUILD
  COMMAND
  ${CMAKE_COMMAND} -E copy 
  "$<TARGET_RUNTIME_DLLS:OpenKneeboard>"
  "$<TARGET_FILE_DIR:OpenKneeboard>"
  COMMAND_EXPAND_LISTS)
