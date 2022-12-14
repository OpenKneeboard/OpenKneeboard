ok_add_library(
  OpenKneeboard-games
  STATIC
  DCSWorld.cpp
  DCSWorldInstance.cpp
  Game.cpp
  GenericGame.cpp
  GameInstance.cpp
)
target_include_directories(OpenKneeboard-games PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(OpenKneeboard-games PUBLIC OpenKneeboard-UTF8 ThirdParty::JSON)
target_link_libraries(OpenKneeboard-games PRIVATE OpenKneeboard-dprint)
