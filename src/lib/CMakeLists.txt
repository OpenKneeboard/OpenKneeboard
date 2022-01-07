add_library(_libheaders INTERFACE)
target_include_directories(_libheaders INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(openkneeboard-shm STATIC SHM.cpp)
target_link_libraries(openkneeboard-shm PRIVATE fmt)
target_link_libraries(openkneeboard-shm PUBLIC _libheaders)

add_library(dprint STATIC dprint.cpp)
target_link_libraries(dprint PUBLIC fmt)
target_link_libraries(dprint PUBLIC _libheaders)

add_library(consolelib STATIC ConsoleLoopCondition.cpp)
target_link_libraries(consolelib PUBLIC _libheaders)

add_library(gameevent STATIC GameEvent.cpp)
target_link_libraries(gameevent PRIVATE fmt dprint)
target_link_libraries(gameevent PUBLIC _libheaders)

add_library(openkneeboard-D2DErrorRenderer STATIC D2DErrorRenderer.cpp)
target_link_libraries(
  openkneeboard-D2DErrorRenderer
  PUBLIC
  D2d1
  Dwrite
  _libheaders)
