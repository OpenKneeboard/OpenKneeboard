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

add_library(yavrk-shm STATIC SHM.cpp)
target_include_directories(yavrk-shm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(yavrk-shm fmt)

add_subdirectory(injectables)

add_library(yavrk-tab-api STATIC Tab.cpp)
target_include_directories(yavrk-tab-api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(yavrk-tab-api PUBLIC wx::core wx::base)

add_library(yavrk-tabs STATIC FolderTab.cpp)
target_link_libraries(yavrk-tabs PUBLIC yavrk-tab-api)

add_executable(demo-feeder demo-feeder.cpp)
target_link_libraries(demo-feeder yavrk-shm)

add_executable(kobuleti-feeder kobuleti-feeder.cpp)
target_link_libraries(kobuleti-feeder yavrk-shm)

add_executable(yavrk WIN32 main.cpp TabWidget.cpp)
target_link_libraries(yavrk PRIVATE wx::core wx::base yavrk-shm yavrk-tabs)
