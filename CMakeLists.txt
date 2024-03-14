cmake_minimum_required(VERSION 3.23)

set(PROJECT_REVERSE_DOMAIN "com.openkneeboard" CACHE STRING "Project reverse domain, e.g. com.example; used for IPC")
set(PROJECT_OPENXR_API_LAYER_NAME "XR_APILAYER_FREDEMMOTT_OpenKneeboard" CACHE STRING "OpenXR API layer name")
set(PROJECT_OPENXR_API_LAYER_DESCRIPTION "OpenKneeboard - https://github.com/OpenKneeboard/OpenKneeboard" CACHE STRING "Project description for API layer JSON")

# Required for ID3D11On12Device2
# set(MINIMUM_WINDOWS_VERSION "10.0.19041.0")
# Required for GraphicsCaptureItem.TryCreateFromWindowId
set(MINIMUM_WINDOWS_VERSION "10.0.20348.0")
set(CMAKE_SYSTEM_VERSION "${MINIMUM_WINDOWS_VERSION}" CACHE INTERNAL "Minimum Windows Version" FORCE)
set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "${MINIMUM_WINDOWS_VERSION}" CACHE INTERNAL "Minimum windows version" FORCE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# Enable CMAKE_MSVC_RUNTIME_LIBRARY variable
cmake_policy(SET CMP0091 NEW)

option(WITH_ASAN "Build with ASAN" OFF)

set(COMMON_COMPILE_OPTIONS)
set(COMMON_LINK_OPTIONS)

if(WITH_ASAN)
  list(
    APPEND COMMON_COMPILE_OPTIONS
    "/fsanitize=address"
  )
  list(
    APPEND COMMON_LINK_OPTIONS

    # Some targets specify incremental linking, which is incompatible
    # with ASAN; this raises a linker warning. Some projects also
    # convert linker warnings to errors.
    "/WX:NO"
  )

  # Link everything in a normal way so the ASAN stubs get detected
  # correctly so the ASAN stubs get detected
  set(
    CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )
  set(
    MSVC_RUNTIME_LIBRARY_COMPILE_OPTION
    "/MT$<$<CONFIG:Debug>:d>"
  )

else() # (not) WITH_ASAN
  # We're going to use the 'Hybrid CRT' approach, which is the combination of the
  # UCRT and the static C++ Runtime
  #
  # https://github.com/microsoft/WindowsAppSDK/blob/main/docs/Coding-Guidelines/HybridCRT.md
  set(
    CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )

  # For backwards-compat with third-party projects that don't have CMP0091 set
  set(
    MSVC_RUNTIME_LIBRARY_COMPILE_OPTION
    "/MT$<$<CONFIG:Debug>:d>"
  )
  list(
    APPEND COMMON_LINK_OPTIONS
    "/DEFAULTLIB:ucrt$<$<CONFIG:Debug>:d>.lib" # include the dynamic UCRT
    "/NODEFAULTLIB:libucrt$<$<CONFIG:Debug>:d>.lib" # remove the static UCRT
  )
endif()

add_link_options("${COMMON_LINK_OPTIONS}")

if(DEFINED ENV{GITHUB_RUN_NUMBER})
  set(VERSION_BUILD $ENV{GITHUB_RUN_NUMBER})
else()
  # Make it so that local builds are easily identifiable, and don't intefere with
  # upgrade logic
  set(VERSION_BUILD 0)
endif()

project("${PROJECT_REVERSE_DOMAIN}" VERSION 1.7.3.${VERSION_BUILD} LANGUAGES CXX C)

message(STATUS "Building OpenKneeboard v${CMAKE_PROJECT_VERSION}")

message(STATUS "CMake Generator: ${CMAKE_GENERATOR}")

STRING(REGEX MATCH "^Visual Studio" GENERATOR_IS_VS "${CMAKE_GENERATOR}")
if (NOT GENERATOR_IS_VS)
  message(FATAL_ERROR "The CMake generator '${CMAKE_GENERATOR}' is not supported; use the Visual Studio generator instead.")
endif()

# Handy for CI
file(WRITE "${CMAKE_BINARY_DIR}/version.txt" "${CMAKE_PROJECT_VERSION}")

if(MSVC AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CLANG_CL 1 CACHE BOOL "Whether we're using clang-cl")
else()
  set(CLANG_CL 0)
endif()

message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Simulated compiler: ${CMAKE_CXX_SIMULATE_ID}")
message(STATUS "MSVC: ${MSVC}")
message(STATUS "CLANG_CL: ${CLANG_CL}")

if(MSVC AND NOT CLANG_CL)
  list(
    APPEND COMMON_COMPILE_OPTIONS

    # Standards-compliant C++20 coroutines
    "/await:strict"

    # Synchronous writes to PDB files in case building with parallel CL.exe
    "/FS"

    # Include content and marker in error messages
    "/diagnostics:caret"
  )
endif()

add_compile_options(${COMMON_COMPILE_OPTIONS})

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME Default)

include(windows_kits_dir.cmake)
include(sign_target.cmake)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.props"
  "${CMAKE_CURRENT_BINARY_DIR}/Directory.Build.props"
  COPYONLY
)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(BUILD_BITNESS 32 CACHE INTERNAL "")
else()
  set(BUILD_BITNESS 64 CACHE INTERNAL "")
endif()

add_subdirectory("scripts")
add_subdirectory("third-party")
add_subdirectory("data")
add_subdirectory("src")

install(
  FILES LICENSE
  TYPE DOC
  RENAME "LICENSE.txt"
)
install(
  FILES gpl-2.0.txt
  TYPE DOC
)

include(dualarch.cmake)

add_subdirectory("installer")
