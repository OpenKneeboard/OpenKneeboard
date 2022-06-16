cmake_minimum_required(VERSION 3.23)

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
  "/DEFAULTLIB:ucrt$<$<CONFIG:Debug>:d>.lib" # include the dynamic UCRT
  "/NODEFAULTLIB:libucrt$<$<CONFIG:Debug>:d>.lib" # remove the static UCRT
)
add_link_options("${COMMON_LINK_OPTIONS}")

project(com.fredemmott.openkneeboard VERSION 1.1.0 LANGUAGES CXX C)

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME Default)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.props"
	"${CMAKE_CURRENT_BINARY_DIR}/Directory.Build.props"
	COPYONLY
)

add_subdirectory("third-party")
add_subdirectory("src")
add_subdirectory("assets")

install(
  FILES
  LICENSE
  gpl-2.0.txt
  TYPE DOC)

# VS_PACKAGE_REFERENCES is not inherited, so we can't create a handy cmake target
# referencing the cppwinrt package - but we need to be certain that every target
# that does `#include <winrt/foo.h>` uses the same version of C++/WinRT, instead
# of some other version, e.g. installed via vcpkg or from the windows SDK

macro(get_all_targets TARGETS DIR)
  get_property(DIR_TARGETS DIRECTORY ${DIR} PROPERTY BUILDSYSTEM_TARGETS)
  list(APPEND "${TARGETS}" ${DIR_TARGETS})

  get_property(SUBDIRS DIRECTORY ${DIR} PROPERTY SUBDIRECTORIES)
  foreach(SUBDIR ${SUBDIRS})
    get_all_targets(${TARGETS} ${SUBDIR})
  endforeach()
endmacro()

set(ALL_TARGETS)
get_all_targets(ALL_TARGETS "${CMAKE_CURRENT_SOURCE_DIR}")

foreach(TARGET ${ALL_TARGETS})
  get_property(NUGET_PACKAGES TARGET "${TARGET}" PROPERTY VS_PACKAGE_REFERENCES)
  list(APPEND NUGET_PACKAGES "Microsoft.Windows.CppWinRT_2.0.220608.4")
  set_property(TARGET "${TARGET}" PROPERTY VS_PACKAGE_REFERENCES ${NUGET_PACKAGES})
endforeach()