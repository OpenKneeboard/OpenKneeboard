cmake_minimum_required(VERSION 3.13)

# Require that targets exist
cmake_policy(SET CMP0079 NEW)

# We're going to use the 'Hybrid CRT' approach, which is the combination of the
# UCRT and the static C++ Runtime
#
# https://github.com/microsoft/WindowsAppSDK/blob/main/docs/Coding-Guidelines/HybridCRT.md

# Enable CMAKE_MSVC_RUNTIME_LIBRARY variable
cmake_policy(SET CMP0091 NEW)
set(
  CMAKE_MSVC_RUNTIME_LIBRARY
  "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
# For backwards-compat with third-party projects that don't have CMP0091 set
set(
  MSVC_RUNTIME_LIBRARY_COMPILE_OPTION
  "/MT$<$<CONFIG:Debug>:d>"
)
set(
  COMMON_LINK_OPTIONS
  "/DEFAULTLIB:ucrt$<$<CONFIG:Debug>:d>.lib" # include the static CRT
  "/NODEFAULTLIB:libucrt$<$<CONFIG:Debug>:d>.lib" # ignore conflicts with dynamic CRT
)
add_link_options("${COMMON_LINK_OPTIONS}")

project(com.fredemmott.openkneeboard VERSION 1.1.0 LANGUAGES CXX C)

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME Default)

add_subdirectory("third-party")
add_subdirectory("src")
add_subdirectory("assets")

install(
  FILES
  LICENSE
  gpl-2.0.txt
  TYPE DOC)
