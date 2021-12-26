add_executable(
  OpenKneeboard
  WIN32
  main.cpp TabCanvasWidget.cpp TabWidget.cpp GameEventNamedPipeListener.cpp)
target_link_libraries(
  OpenKneeboard
  PRIVATE
  wx::core
  wx::base
  openkneeboard-shm
  openkneeboard-tabs
  dprint)
