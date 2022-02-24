cmake_minimum_required(VERSION 3.13)
# Require that targets exist
cmake_policy(SET CMP0079 NEW)

project(com.fredemmott.openkneeboard VERSION 0.1.3 LANGUAGES CXX C)

add_subdirectory("third-party")
add_subdirectory("src")
add_subdirectory("assets")

install(
  FILES
  LICENSE
  gpl-2.0.txt
  TYPE DOC)
