add_library(_dcsHeaders INTERFACE)
target_include_directories(_dcsHeaders INTERFACE "${CMAKE_SOURCE_DIR}/src/include")

add_library(OpenKneeboardDCSExt MODULE OpenKneeboardDCSExt.cpp)
target_link_libraries(OpenKneeboardDCSExt PUBLIC lualib)
target_link_libraries(OpenKneeboardDCSExt PRIVATE lualib fmt dprint gameevent _dcsHeaders)
