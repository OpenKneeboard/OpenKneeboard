add_library(
  OpenKneeboard-games
  STATIC
  DCSWorld.cpp
  Game.cpp
  GenericGame.cpp
)
target_include_directories(OpenKneeboard-games PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(OpenKneeboard-games PUBLIC wx::core wx::base)
target_link_libraries(OpenKneeboard-games PRIVATE fmt OpenKneeboard-dprint)
