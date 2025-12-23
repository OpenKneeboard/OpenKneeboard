cmake_minimum_required(VERSION 3.25..4.0 FATAL_ERROR)

# `GIT_SUBMODULES ""` means no submodules
cmake_policy(SET CMP0097 NEW)

# cmake --build . --preset appears to require CMakePresets exists in the build directory,
# not the source directory, as of CMake 4.0 (2025-08-21)
#
# While we're here, let's set the parallelism nicely :)
if (NOT "${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  cmake_host_system_information(RESULT CORE_COUNT QUERY NUMBER_OF_LOGICAL_CORES)
  file(READ "${CMAKE_SOURCE_DIR}/CMakePresets.json" PRESETS)
  string(REGEX REPLACE "\"jobs\": \\d+" "\"jobs\": ${CORE_COUNT}" PRESETS "${PRESETS}")
  file(WRITE "${CMAKE_BINARY_DIR}/CMakePresets.json" "${PRESETS}")
endif ()

set(PROJECT_REVERSE_DOMAIN "com.openkneeboard" CACHE STRING "Project reverse domain, e.g. com.example; used for IPC")
set(PROJECT_OPENXR_API_LAYER_NAME "XR_APILAYER_FREDEMMOTT_OpenKneeboard" CACHE STRING "OpenXR API layer name")
set(PROJECT_OPENXR_API_LAYER_DESCRIPTION "OpenKneeboard - https://github.com/OpenKneeboard/OpenKneeboard" CACHE STRING "Project description for API layer JSON")

# Win10 19H1 runtime-required for:
# - AF_UNIX (OTD-IPC v2)
# - icu.dll

# For later NTDDI_VERSION values, see `sdkddkver.h`
# https://en.wikipedia.org/wiki/List_of_Microsoft_codenames is a useful reference - e.g. `NTDDI_WIN10_VB` is probably
# 'Vibranium', a.k.a Win20 20H1
set(NTDDI_VERSION "NTDDI_WIN10_19H1")
set(WIN32_WINNT "_WIN32_WINNT_WIN10")

# Win10 20H1 SDK (20348) build-time-required for:
# - GraphicsCaptureItem.TryCreateFromWindowId
set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "10.0.20348.0" CACHE INTERNAL "Minimum windows version" FORCE)

set(CMAKE_CXX_STANDARD 23)
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

set(
  COMMON_COMPILE_OPTIONS
  # Disable iterator debugging for CEF USE_SANDBOX
  "-D_HAS_ITERATOR_DEBUGGING=0"
  # _HAS_ITERATOR_DEBUGGING is set by CEF but deprecated,
  # set the non-deprecated property too
  "-D_ITERATOR_DEBUG_LEVEL=0"
  "/D_WIN32_WINNT=${WIN32_WINNT}"
  "/DNTDDI_VERSION=${NTDDI_VERSION}"
)
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

set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo)

project(OpenKneeboard VERSION 1.13.0.${VERSION_BUILD} LANGUAGES CXX C)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(smol_binaries)

set(BUILD_OUT_PREFIX "${CMAKE_BINARY_DIR}/out" CACHE PATH "Location for final build artifacts")
get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(GENERATOR_IS_MULTI_CONFIG)
  set(BUILD_OUT_ROOT "${BUILD_OUT_PREFIX}/$<CONFIG>")
else()
  set(BUILD_OUT_ROOT "${BUILD_OUT_PREFIX}/${CMAKE_BUILD_TYPE}")
endif()
set(BUILD_OUT_BINDIR "${BUILD_OUT_ROOT}/bin")
set(BUILD_OUT_PDBDIR "${BUILD_OUT_ROOT}/pdb")

# The "implementation_version" field of the layer's JSON file is a string, but it should
# match the `uint32_t` field of XrApiLayerProperties.
#
# The specification is inconsistent about whether this should be for 'any major changes'
# or 'backwards compatible' (sic?) changes. We also can't fit the full version number in
# that, so let's just go for:
# - 8 bits for the major version
# - 24 bits for the minor version
math(EXPR OPENXR_API_LAYER_IMPLEMENTATION_VERSION "(${CMAKE_PROJECT_VERSION_MAJOR} << 24) | ${CMAKE_PROJECT_VERSION_MINOR}")
message(STATUS "OpenXR API layer implementation version: ${OPENXR_API_LAYER_IMPLEMENTATION_VERSION}")

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
  add_compile_definitions("CLANG_CL=1")
else()
  set(CLANG_CL 0)
endif()

# As of 2024-07-24:
# - CMake automatically converts this to `/std:c++latest` for MSVC
# - clang-cl doesn't treat that as equivalent or `c++23`
if(CLANG_CL)
  add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:/clang:--std=c++23>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-command-line-argument>"
  )
endif()

# MSBuild ignores CMAKE_CXX_COMPILER and friends
set(MSBUILD_LLVM_ROOT "" CACHE PATH "Change LLVM binary used by MSBuild; you must also set MSBUILD_LLVM_VERSION")
set(MSBUILD_LLVM_VERSION "" CACHE STRING "LLVM version used by MSBuild; you must also set MSBUILD_LLVM_ROOT")
if(MSBUILD_LLVM_ROOT)
  list(
    APPEND CMAKE_VS_GLOBALS
    "LLVMInstallDir=${MSBUILD_LLVM_ROOT}"
    "LLVMToolsVersion=${MSBUILD_LLVM_VERSION}"
  )
endif() 

message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Simulated compiler: ${CMAKE_CXX_SIMULATE_ID}")
message(STATUS "MSVC: ${MSVC}")
message(STATUS "CLANG_CL: ${CLANG_CL}")

if(MSVC AND NOT CLANG_CL)
  list(
    APPEND COMMON_COMPILE_OPTIONS

    # Synchronous writes to PDB files in case building with parallel CL.exe
    "/FS"

    # Include content and marker in error messages
    "/diagnostics:caret"
  )
endif()

add_compile_options(${COMMON_COMPILE_OPTIONS})

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME Default)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.props"
  "${CMAKE_CURRENT_BINARY_DIR}/Directory.Build.props"
  COPYONLY
)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(BUILD_BITNESS 32 CACHE INTERNAL "")
  set(BUILD_IS_32BIT ON CACHE INTERNAL "")
else()
  set(BUILD_BITNESS 64 CACHE INTERNAL "")
  set(BUILD_IS_64BIT ON CACHE INTERNAL "")
endif()

option(ENABLE_APP "Build the main app, not just the injectables" ON)

add_subdirectory("cmake")

add_subdirectory("scripts")
add_subdirectory("third-party")
add_subdirectory("data")
add_subdirectory("src")

# MUST be set in this CMakeLists.txt to work, not a subfolder
set_property(GLOBAL PROPERTY VS_STARTUP_PROJECT OpenKneeboard-App-WinUI3)

ok_add_license_file(LICENSE LICENSE.txt)

add_subdirectory(OpenKneeboard-Installer)