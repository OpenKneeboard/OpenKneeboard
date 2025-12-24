# OpenKneeboard
#
# Copyright (C) 2025 Fred Emmott <fred@fredemmott.com>
#
# This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

include_guard(GLOBAL)

if (NOT ENABLE_APP)
    return()
endif ()

set(CEF_VERSION "132.3.1+g144febe+chromium-132.0.6834.83")
set(CEF_DOWNLOAD_ROOT "${CMAKE_BINARY_DIR}/_cef")
set(CEF_BINARY_OUT_DIR "${BUILD_OUT_ROOT}/libexec/cef")
set(CEF_RESOURCE_OUT_DIR "${BUILD_OUT_ROOT}/libexec/cef")

include(ok_add_license_file)
ok_add_license_file(cef/LICENSE.txt LICENSE-ThirdParty-CEF.txt)

include(cef/DownloadCEF.cmake)
DownloadCEF("windows64" "${CEF_VERSION}" "${CEF_DOWNLOAD_ROOT}")
message(STATUS "Downloaded Chromium Embedded Framework (CEF) to ${CEF_ROOT}")

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")

find_package(CEF REQUIRED)

# CEF requires compatibility manifests; we provide the necessary manifest
list(REMOVE_ITEM CEF_EXE_LINKER_FLAGS "/MANIFEST:NO")
list(REMOVE_ITEM CEF_COMPILER_DEFINES "_HAS_EXCEPTIONS=0")

PRINT_CEF_CONFIG()
message(STATUS "*** END CEF CONFIGURATION SETTINGS ***")

set(CEF_BINARY_DIR "$<IF:$<CONFIG:Debug>,${CEF_BINARY_DIR_DEBUG},${CEF_BINARY_DIR_RELEASE}>>")

# Not using CEF's helper macros as:
# - they only support the 'Release' config, not 'RelWithDebInfo'
# - ADD_LOGICAL_TARGET, SET_EXECUTABLE_TARGET_PROPERTIES and friends are extremely generically named
add_library(
    Cef::LibCef
    SHARED
    IMPORTED
    GLOBAL
)
set_target_properties(
    Cef::LibCef
    PROPERTIES
    IMPORTED_IMPLIB_DEBUG "${CEF_LIB_DEBUG}"
    IMPORTED_IMPLIB_RELEASE "${CEF_LIB_RELEASE}"
    IMPORTED_IMPLIB_RELWITHDEBINFO "${CEF_LIB_RELEASE}"
)
# Intentionally not adding CEF_CXX_COMPILER_FLAGS: this adds several that are appropriate for CEF's own targets,
# but not to force on for things that *use* CEF, e.g. /W4;/WX
#
# CEF_CXX_COMPILER_FLAGS also adds /std:c++17 which we don't want as we use c++23; re-adding it as a *lower*
# bound with target_compile_features() below
target_compile_options(
    Cef::LibCef
    INTERFACE
    "$<IF:$<CONFIG:Debug>,${CEF_COMPILER_FLAGS_DEBUG},${CEF_COMPILER_FLAGS_RELEASE}>"
)
# Restore Windows/MS defaults
#
# Chromium itself has unacceptable performance - even for debugging - with _ITERATOR_DEBUG_LEVEL; however, that
# doesn't really apply to the DLL wrapper
#
# These need to match across the *entire* build - so, keeping the defaults meant every dependency *except* CEF
# needed to be built differently
list(
  REMOVE_ITEM CEF_COMPILER_DEFINES_DEBUG
  _HAS_ITERATOR_DEBUGGING=0
)
# Requires the previous
list(REMOVE_ITEM CEF_COMPILER_DEFINES CEF_USE_SANDBOX)
list(APPEND CEF_COMPILER_DEFINES "$<$<NOT:$<CONFIG:Debug>>:CEF_USE_SANDBOX>")

target_compile_definitions(
    Cef::LibCef
    INTERFACE
    "${CEF_COMPILER_DEFINES}"
    "$<IF:$<CONFIG:Debug>,${CEF_COMPILER_DEFINES_DEBUG},${CEF_COMPILER_DEFINES_RELEASE}>"
)
target_compile_features(
    Cef::LibCef
    INTERFACE
    cxx_std_17
)
target_include_directories(
    Cef::LibCef
    INTERFACE
    "${CEF_INCLUDE_PATH}"
)
target_link_options(
    Cef::LibCef
    INTERFACE
    "${CEF_LINKER_FLAGS}"
    "$<IF:$<CONFIG:Debug>,${CEF_LINKER_FLAGS_DEBUG},${CEF_LINKER_FLAGS_RELEASE}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${CEF_EXE_LINKER_FLAGS};$<IF:$<CONFIG:Debug>,${CEF_EXE_LINKER_FLAGS_DEBUG},${CEF_EXE_LINKER_FLAGS_RELEASE}>>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${CEF_SHARED_LINKER_FLAGS};$<IF:$<CONFIG:Debug>,${CEF_SHARED_LINKER_FLAGS_DEBUG},${CEF_SHARED_LINKER_FLAGS_RELEASE}>>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${CEF_SHARED_LINKER_FLAGS};$<IF:$<CONFIG:Debug>,${CEF_SHARED_LINKER_FLAGS_DEBUG},${CEF_SHARED_LINKER_FLAGS_RELEASE}>>"
)

add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" "${CMAKE_BINARY_DIR}/_cef/libcef_dll_wrapper" EXCLUDE_FROM_ALL)
set_target_properties(
    libcef_dll_wrapper
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CEF_BINARY_OUT_DIR}"
    PDB_OUTPUT_DIRECTORY "${BUILD_OUT_PDBDIR}"
)
target_link_libraries(
    Cef::LibCef
    INTERFACE
    libcef_dll_wrapper
    "$<$<NOT:$<CONFIG:Debug>>:${CEF_SANDBOX_LIB_RELEASE};${CEF_SANDBOX_STANDARD_LIBS}>"
)

add_custom_target(
    copy-cef-binaries
    COMMAND
    "${CMAKE_COMMAND}"
    -E make_directory
    "${CEF_BINARY_OUT_DIR}"
    COMMAND
    "${CMAKE_COMMAND}"
    -E copy_if_different
    "${CEF_BINARY_FILES}"
    "${CEF_BINARY_OUT_DIR}"
    WORKING_DIRECTORY "${CEF_BINARY_DIR}"
    VERBATIM
    COMMAND_EXPAND_LISTS
)
list(REMOVE_ITEM CEF_RESOURCE_FILES locales)
add_custom_target(
    copy-cef-resources
    COMMAND
    "${CMAKE_COMMAND}"
    -E make_directory
    "${CEF_RESOURCE_OUT_DIR}"
    COMMAND
    "${CMAKE_COMMAND}"
    -E copy_if_different
    "${CEF_RESOURCE_FILES}"
    "${CEF_RESOURCE_OUT_DIR}"
    WORKING_DIRECTORY "${CEF_RESOURCE_DIR}"
    VERBATIM
    COMMAND_EXPAND_LISTS
)
file(GLOB CEF_LOCALES "${CEF_RESOURCE_DIR}/locales/*")
add_custom_target(
    copy-cef-locales
    COMMAND
    "${CMAKE_COMMAND}"
    -E make_directory
    "${CEF_RESOURCE_OUT_DIR}/locales"
    COMMAND
    "${CMAKE_COMMAND}"
    -E copy_if_different
    "${CEF_LOCALES}"
    "${CEF_RESOURCE_OUT_DIR}/locales"
    WORKING_DIRECTORY "${CEF_RESOURCE_DIR}/locales"
    VERBATIM
    COMMAND_EXPAND_LISTS
)
add_dependencies(
    Cef::LibCef
    copy-cef-binaries
    copy-cef-resources
    copy-cef-locales
)
