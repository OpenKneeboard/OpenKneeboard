add_library(openkneeboard-tab-api STATIC Tab.cpp)
target_include_directories(openkneeboard-tab-api PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(openkneeboard-tab-api PUBLIC wx::core wx::base)

add_library(openkneeboard-tabs STATIC FolderTab.cpp)
target_link_libraries(openkneeboard-tabs PUBLIC openkneeboard-tab-api)
