add_library(yavrk-shm STATIC SHM.cpp)
target_link_libraries(yavrk-shm PRIVATE fmt)
target_include_directories(yavrk-shm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)