set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (MSVC)
  add_definitions("/Zc:__cplusplus" "/DUNICODE=1" "/DNOMINMAX" "/DWIN32_LEAN_AND_MEAN" "/await")
endif()

add_definitions(
  "-DOPENKNEEBOARD_TEXTURE_WIDTH=768"
  "-DOPENKNEEBOARD_TEXTURE_HEIGHT=1024"
)

add_library(headers INTERFACE)
target_include_directories(headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

##### libraries shared between multiple apps/dlls #####

add_library(openkneeboard-shm STATIC SHM.cpp)
target_link_libraries(openkneeboard-shm fmt headers)

add_library(dprint STATIC dprint.cpp)
target_link_libraries(dprint PUBLIC fmt headers)

##### dlls we inject into other processes #####

add_subdirectory(injectables)
add_subdirectory(dcs)

##### components of the GUI app #####

add_library(openkneeboard-tab-api STATIC Tab.cpp)
target_link_libraries(openkneeboard-tab-api PUBLIC wx::core wx::base headers)

add_library(openkneeboard-tabs STATIC FolderTab.cpp)
target_link_libraries(openkneeboard-tabs PUBLIC openkneeboard-tab-api)

##### the gui app itself #####

add_executable(openkneeboard WIN32 main.cpp TabCanvasWidget.cpp TabWidget.cpp GameEventNamedPipeListener.cpp)
target_link_libraries(openkneeboard PRIVATE wx::core wx::base openkneeboard-shm openkneeboard-tabs dprint)

##### debug/test apps #####

add_executable(test-feeder test-feeder.cpp ConsoleLoopCondition.cpp)
target_link_libraries(test-feeder openkneeboard-shm)

add_executable(test-viewer WIN32 test-viewer.cpp)
target_link_libraries(test-viewer openkneeboard-shm wx::core wx::base)
