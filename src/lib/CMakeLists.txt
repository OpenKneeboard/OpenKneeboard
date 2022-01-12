add_library(_libheaders INTERFACE)
target_include_directories(_libheaders INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(RUNTIME_FILES_CPP "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/RuntimeFiles.cpp")
file(GENERATE OUTPUT "${RUNTIME_FILES_CPP}" INPUT RuntimeFiles.cpp.in)
add_library(OpenKneeboard-RuntimeFiles STATIC "${RUNTIME_FILES_CPP}")
target_link_libraries(OpenKneeboard-RuntimeFiles PUBLIC _libheaders)

add_library(OpenKneeboard-SHM STATIC SHM.cpp)
target_link_libraries(OpenKneeboard-SHM PRIVATE fmt)
target_link_libraries(OpenKneeboard-SHM PUBLIC _libheaders)

add_library(OpenKneeboard-dprint STATIC dprint.cpp)
target_link_libraries(OpenKneeboard-dprint PUBLIC fmt)
target_link_libraries(OpenKneeboard-dprint PUBLIC _libheaders)

add_library(OpenKneeboard-consolelib STATIC ConsoleLoopCondition.cpp)
target_link_libraries(OpenKneeboard-consolelib PUBLIC _libheaders)

add_library(OpenKneeboard-GameEvent STATIC GameEvent.cpp)
target_link_libraries(OpenKneeboard-GameEvent PRIVATE fmt OpenKneeboard-dprint)
target_link_libraries(OpenKneeboard-GameEvent PUBLIC _libheaders)

add_library(OpenKneeboard-D2DErrorRenderer STATIC D2DErrorRenderer.cpp)
target_link_libraries(
  OpenKneeboard-D2DErrorRenderer
  PUBLIC
  D2d1
  Dwrite
  _libheaders)
