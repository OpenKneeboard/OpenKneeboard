
add_library(
  games
  STATIC
  Game.cpp
  DCSWorld.cpp)
target_include_directories(games PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(games PUBLIC wx::core wx::base)
target_link_libraries(games PRIVATE fmt openkneeboard-dprint)
