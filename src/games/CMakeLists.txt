
add_library(
  games
  STATIC
  Game.cpp
  DCSWorld.cpp)
target_include_directories(games PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(games PRIVATE wx::core fmt dprint)
