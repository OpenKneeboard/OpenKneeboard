add_subdirectory("injectables")

add_library(yavrk-shm INTERFACE)
target_include_directories(yavrk-shm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})