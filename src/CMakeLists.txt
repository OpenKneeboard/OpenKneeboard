set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (MSVC)
  add_definitions("/Zc:__cplusplus" "/DUNICODE=1" "/DNOMINMAX" "/DWIN32_LEAN_AND_MEAN" "/await")
endif()

add_definitions(
  "-DYAVRK_TEXTURE_WIDTH=768"
  "-DYAVRK_TEXTURE_HEIGHT=1024"
)

##### libraries shared between multiple apps/dlls #####

add_library(yavrk-shm STATIC SHM.cpp)
target_include_directories(yavrk-shm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(yavrk-shm fmt)

##### dlls we inject into other processes #####

add_subdirectory(injectables)

##### components of the GUI app #####

add_library(yavrk-tab-api STATIC Tab.cpp)
target_include_directories(yavrk-tab-api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(yavrk-tab-api PUBLIC wx::core wx::base)

add_library(yavrk-tabs STATIC FolderTab.cpp)
target_link_libraries(yavrk-tabs PUBLIC yavrk-tab-api)

##### the gui app itself #####

add_executable(yavrk WIN32 main.cpp TabCanvasWidget.cpp TabWidget.cpp)
target_link_libraries(yavrk PRIVATE wx::core wx::base yavrk-shm yavrk-tabs)

##### debug/test apps #####

add_executable(demo-feeder demo-feeder.cpp ConsoleLoopCondition.cpp)
target_link_libraries(demo-feeder yavrk-shm)

add_executable(kobuleti-feeder kobuleti-feeder.cpp)
target_link_libraries(kobuleti-feeder yavrk-shm)

add_executable(test-viewer WIN32 test-viewer.cpp)
target_link_libraries(test-viewer yavrk-shm wx::core wx::base)
