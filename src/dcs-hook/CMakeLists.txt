add_library(openkneeboard-dcs-ext-dll MODULE OpenKneeboardDCSExt.cpp)
target_link_libraries(openkneeboard-dcs-ext-dll PUBLIC lualib)
target_link_libraries(openkneeboard-dcs-ext-dll PRIVATE lualib fmt openkneeboard-dprint openkneeboard-gameevent)
set_target_properties(
  openkneeboard-dcs-ext-dll
  PROPERTIES
  OUTPUT_NAME "OpenKneeboardDCSExt")

set(LUA_FILE_NAME "OpenKneeboardDCSExt.lua")
set(LUA_OUT_DIR "$<TARGET_FILE_DIR:openkneeboard-dcs-ext-dll>")
set(LUA_OUT_PATH "${LUA_OUT_DIR}/${LUA_FILE_NAME}")

add_custom_target(openkneeboard-dcs-ext-lua ALL SOURCES "${LUA_FILE_NAME}")
add_custom_command(
  TARGET
  openkneeboard-dcs-ext-lua
  POST_BUILD
  COMMAND
  ${CMAKE_COMMAND} -E copy_if_different
  "${CMAKE_CURRENT_SOURCE_DIR}/${LUA_FILE_NAME}"
  "${LUA_OUT_PATH}")

set_target_properties(
  openkneeboard-dcs-ext-lua
  PROPERTIES
  OUTPUT_NAME "${LUA_FILE_NAME}")
