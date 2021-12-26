add_executable(test-feeder test-feeder.cpp)
target_link_libraries(test-feeder openkneeboard-shm consolelib)

add_executable(test-viewer WIN32 test-viewer.cpp)
target_link_libraries(test-viewer openkneeboard-shm wx::core wx::base)
