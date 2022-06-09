cmake_minimum_required(VERSION 3.13)
# Require that targets exist
cmake_policy(SET CMP0079 NEW)

cmake_policy(SET CMP0091 NEW)
option(USE_STATIC_CPP_RUNTIME "Use static MSVC runtime" OFF)
set(
  CMAKE_MSVC_RUNTIME_LIBRARY
  "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<NOT:$<BOOL:${USE_STATIC_CPP_RUNTIME}>>:DLL>"
)

project(com.fredemmott.openkneeboard VERSION 1.1.0 LANGUAGES CXX C)

add_subdirectory("third-party")
add_subdirectory("src")
add_subdirectory("assets")

install(
  FILES
  LICENSE
  gpl-2.0.txt
  TYPE DOC)
