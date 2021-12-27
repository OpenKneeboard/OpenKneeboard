add_library(OpenKneeboardDCSExt MODULE OpenKneeboardDCSExt.cpp)
target_link_libraries(OpenKneeboardDCSExt PUBLIC lualib)
target_link_libraries(OpenKneeboardDCSExt PRIVATE lualib fmt dprint gameevent)

# Just for convenience when developing: both the .dll and .lua file need copying
# to Saved Games\DCS\Scripts\Hooks, so put them in the same place.
add_custom_target(DCSLua)
add_custom_command(
  TARGET DCSLua
  COMMAND
  ${CMAKE_COMMAND} -E copy
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenKneeboardDCSExt.lua"
  "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/OpenKneeboardDCSExt.lua")

add_dependencies(OpenKneeboardDCSExt DCSLua)
