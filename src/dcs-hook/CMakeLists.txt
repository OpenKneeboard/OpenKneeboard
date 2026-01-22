ok_add_runtime_files(dcs-lua-ext "${CMAKE_CURRENT_SOURCE_DIR}/OpenKneeboardDCSExt.lua")
# Used in RuntimeFiles.cpp.in
set_target_properties(
  dcs-lua-ext
  PROPERTIES
  OUTPUT_NAME "OpenKneeboardDCSExt.lua"
)