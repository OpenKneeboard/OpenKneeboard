cmake_minimum_required(VERSION 3.10)

project(com.fredemmott.openkneeboard VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (MSVC)
  add_definitions("/Zc:__cplusplus" "/DUNICODE=1" "/DNOMINMAX" "/DWIN32_LEAN_AND_MEAN" "/await")
endif()

add_subdirectory("third-party")
add_subdirectory("src")