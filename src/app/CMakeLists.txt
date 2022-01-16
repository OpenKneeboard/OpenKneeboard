file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(OpenKneeboard WIN32 ${SOURCES})
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
  OpenKneeboard-dprint
  OpenKneeboard-GameEvent
  games
  json
  OpenKneeboard-SHM
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-D2DErrorRenderer
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

add_custom_target(
  CopyRuntimeFiles
  ALL
  COMMAND
  ${CMAKE_COMMAND} -E copy_if_different
	"$<TARGET_RUNTIME_DLLS:OpenKneeboard>"
	"$<TARGET_FILE:OpenKneeboard-InjectionBootstrapper>"
	"$<TARGET_FILE:OpenKneeboard-AutoInjectMarker>"
	"$<TARGET_FILE:OpenKneeboard-nonvr-d3d11>"
	"$<TARGET_FILE:OpenKneeboard-oculus-d3d11>"
	"$<TARGET_FILE:OpenKneeboard-dcs-ext-dll>"
	"$<TARGET_FILE_DIR:OpenKneeboard-dcs-ext-dll>/$<TARGET_PROPERTY:OpenKneeboard-dcs-ext-lua,OUTPUT_NAME>"
  "$<TARGET_FILE_DIR:OpenKneeboard>"
  &&
  ${CMAKE_COMMAND} -E touch RuntimeFiles.stamp
  COMMAND_EXPAND_LISTS)
