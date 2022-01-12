add_library(OpenKneeboard-dcs-ext-dll MODULE OpenKneeboardDCSExt.cpp)
target_link_libraries(OpenKneeboard-dcs-ext-dll PUBLIC lualib)
target_link_libraries(OpenKneeboard-dcs-ext-dll PRIVATE lualib fmt OpenKneeboard-dprint OpenKneeboard-GameEvent)
set_target_properties(
  OpenKneeboard-dcs-ext-dll
  PROPERTIES
  OUTPUT_NAME "OpenKneeboardDCSExt")

set(LUA_FILE_NAME "OpenKneeboardDCSExt.lua")
set(LUA_OUT_DIR "$<TARGET_FILE_DIR:OpenKneeboard-dcs-ext-dll>")
set(LUA_OUT_PATH "${LUA_OUT_DIR}/${LUA_FILE_NAME}")

add_custom_target(OpenKneeboard-dcs-ext-lua ALL SOURCES "${LUA_FILE_NAME}")
add_custom_command(
  TARGET
  OpenKneeboard-dcs-ext-lua
  POST_BUILD
  COMMAND
  ${CMAKE_COMMAND} -E copy_if_different
  "${CMAKE_CURRENT_SOURCE_DIR}/${LUA_FILE_NAME}"
  "${LUA_OUT_PATH}")

set_target_properties(
  OpenKneeboard-dcs-ext-lua
  PROPERTIES
  OUTPUT_NAME "${LUA_FILE_NAME}")
