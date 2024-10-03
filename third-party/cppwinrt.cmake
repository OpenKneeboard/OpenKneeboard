include_guard(GLOBAL)

include(ExternalProject)

# Used for nuget
set(CPPWINRT_VERSION "2.0.240111.5" CACHE INTERNAL "")
ExternalProject_Add(
  CppWinRTNuget
  URL "https://www.nuget.org/api/v2/package/Microsoft.Windows.CppWinRT/2.0.240111.5"
  URL_HASH "SHA512=c6c38b81640d7d96d3ca76c321289d6f92eec9bb593a11824640c7dc3651dc69cce1e85ca0324396b4a4d55f790f2c16f835da261e7821137de1eb491b52ffc8"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND
  "<SOURCE_DIR>/bin/cppwinrt.exe"
  -in "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}"
  -output "<BINARY_DIR>/include"
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
message(STATUS "Windows target version: ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")

ExternalProject_Get_property(CppWinRTNuget SOURCE_DIR)
ExternalProject_Get_property(CppWinRTNuget BINARY_DIR)

add_library(CppWinRT INTERFACE)
add_dependencies(CppWinRT CppWinRTNuget)
target_link_libraries(CppWinRT INTERFACE System::WindowsApp)
target_include_directories(CppWinRT SYSTEM INTERFACE "${BINARY_DIR}/include")
add_library(ThirdParty::CppWinRT ALIAS CppWinRT)

add_executable(ThirdParty::CppWinRT::Exe IMPORTED GLOBAL)
add_dependencies(ThirdParty::CppWinRT::Exe CppWinRTNuget)
set_target_properties(
  ThirdParty::CppWinRT::Exe
  PROPERTIES
  IMPORTED_LOCATION "${SOURCE_DIR}/bin/cppwinrt.exe"
)
set_target_properties(
  CppWinRT
  PROPERTIES
  VERSION "${CPPWINRT_VERSION}"
)

install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-CppWinRT.txt"
)
