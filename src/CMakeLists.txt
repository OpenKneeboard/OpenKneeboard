add_subdirectory(YAVRK)
add_subdirectory(injectables)

add_executable(demo-feeder demo-feeder.cpp)
target_link_libraries(demo-feeder yavrk-shm)