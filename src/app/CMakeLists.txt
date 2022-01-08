file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.cpp")

set(RUNTIME_FILES_CPP "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/RuntimeFiles.cpp")
file(GENERATE OUTPUT "${RUNTIME_FILES_CPP}" INPUT RuntimeFiles.cpp.in)
add_executable(OpenKneeboard WIN32 ${SOURCES} ${RUNTIME_FILES_CPP})
target_compile_definitions(
  OpenKneeboard
  PRIVATE
  "DIRECTINPUT_VERSION=0x0800"
)

target_link_libraries(
  OpenKneeboard
  PRIVATE
  D2d1
  directxtk
  detours
  dprint
  gameevent
  games
  json
  openkneeboard-shm
  openkneeboard-D2DErrorRenderer
  openvr
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
  ${CMAKE_COMMAND} -E copy_if_different
  "$<TARGET_RUNTIME_DLLS:OpenKneeboard>"
  "$<TARGET_FILE:openkneeboard-oculus-d3d11>"
  "$<TARGET_FILE_DIR:OpenKneeboard>"
  COMMAND_EXPAND_LISTS)
