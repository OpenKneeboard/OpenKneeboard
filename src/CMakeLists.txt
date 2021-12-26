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

add_library(consolelib STATIC ConsoleLoopCondition.cpp)
targeT_link_libraries(consolelib PUBLIC headers)

##### dlls we inject into other processes #####

add_subdirectory(injectables)
add_subdirectory(dcs)

##### tabs viewable in the kneeboard #####

add_library(openkneeboard-tab-api STATIC Tab.cpp)
target_link_libraries(openkneeboard-tab-api PUBLIC wx::core wx::base headers)

add_library(openkneeboard-tabs STATIC FolderTab.cpp)
target_link_libraries(openkneeboard-tabs PUBLIC openkneeboard-tab-api)

##### apps #####

add_subdirectory(app)
add_subdirectory(utilities)
