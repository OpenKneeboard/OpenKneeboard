ok_add_library(OpenKneeboard-Geometry2D INTERFACE include/OpenKneeboard/Geometry2D.hpp)
target_link_libraries(OpenKneeboard-Geometry2D INTERFACE ThirdParty::felly)
target_include_directories(OpenKneeboard-Geometry2D INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")