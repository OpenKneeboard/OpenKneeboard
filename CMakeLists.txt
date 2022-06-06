cmake_minimum_required(VERSION 3.13)
# Require that targets exist
cmake_policy(SET CMP0079 NEW)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

project(com.fredemmott.openkneeboard VERSION 1.0.0 LANGUAGES CXX C)

add_subdirectory("third-party")
add_subdirectory("src")
add_subdirectory("assets")

install(
  FILES
  LICENSE
  gpl-2.0.txt
  TYPE DOC)
