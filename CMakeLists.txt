cmake_minimum_required(VERSION 3.23)

# Required for ID3D11On12Device2
set(MINIMUM_WINDOWS_VERSION "10.0.19041.0")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_SYSTEM_VERSION "${MINIMUM_WINDOWS_VERSION}")
set(CMAKE_INSTALL_BINDIR "." CACHE STRING "Path to install binaries relative to the root" FORCE)

set(
  CMAKE_VS_GLOBALS
  "AppxPackage=false"
  "RuntimeIdentifier=win10-x64"
  "RuntimeIdentifiers=win10-x64"
  "BaseNuGetRuntimeIdentifier=win10"
  "AssetTargetFallback=$(AssetTargetFallback);native"
)

add_compile_options(
  # Standard C++ exception behavior
  "/EHsc"
)

# Require that targets exist
cmake_policy(SET CMP0079 NEW)
set(CMAKE_LINK_LIBRARIES_ONLY_TARGETS ON)
cmake_policy(SET CMP0028 NEW)

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

set(
  COMMON_COMPILE_OPTIONS
  "/await:strict"
  # Synchronous writes to PDB files in case building with parallel CL.exe
  "/FS"
  # Include content and marker in error messages
  "/diagnostics:caret"
)
add_compile_options(${COMMON_COMPILE_OPTIONS})

if(DEFINED ENV{GITHUB_RUN_NUMBER})
  set(VERSION_BUILD $ENV{GITHUB_RUN_NUMBER})
else()
  execute_process(
    COMMAND git rev-list --count HEAD
    OUTPUT_VARIABLE VERSION_BUILD
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
endif()
project(com.fredemmott.openkneeboard VERSION 1.3.0.${VERSION_BUILD} LANGUAGES CXX C)
message(STATUS "Building OpenKneeboard v${CMAKE_PROJECT_VERSION}")

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME Default)

include(sign_target.cmake)


configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.props"
	"${CMAKE_CURRENT_BINARY_DIR}/Directory.Build.props"
	COPYONLY
)

add_subdirectory("third-party")
add_subdirectory("src")
add_subdirectory("assets")

install(
  FILES LICENSE
  TYPE DOC
  RENAME "LICENSE.txt"
)
install(
  FILES gpl-2.0.txt
  TYPE DOC
)
