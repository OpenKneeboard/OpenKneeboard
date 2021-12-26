add_library(OpenKneeboardDCSExt MODULE OpenKneeboardDCSExt.cpp)
target_link_libraries(OpenKneeboardDCSExt PUBLIC lualib)
target_link_libraries(OpenKneeboardDCSExt PRIVATE lualib fmt dprint)
target_include_directories(OpenKneeboardDCSExt PRIVATE "${CMAKE_SOURCE_DIR}/src/include")
