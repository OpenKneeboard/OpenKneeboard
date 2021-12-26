add_library(YAVRKDCSExt MODULE YAVRKDCSExt.cpp)
target_link_libraries(YAVRKDCSExt PUBLIC lualib)
target_link_libraries(YAVRKDCSExt PRIVATE lualib fmt)
target_include_directories(YAVRKDCSExt PRIVATE "${CMAKE_SOURCE_DIR}/src/include")
