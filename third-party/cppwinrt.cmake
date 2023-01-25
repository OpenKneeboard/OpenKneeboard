include(ExternalProject)

# Used for nuget
set(CPPWINRT_VERSION "2.0.221121.5" CACHE INTERNAL "")
ExternalProject_Add(
  CppWinRTNuget
  URL "https://www.nuget.org/api/v2/package/Microsoft.Windows.CppWinRT/2.0.221121.5"
  URL_HASH "SHA256=14fdc0826b85c0cbb1a9301a4dc0bf18627d6d0e001184a61364bd19fe51f877"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND
  "<SOURCE_DIR>/bin/cppwinrt.exe"
  -in "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}"
  -output "<BINARY_DIR>/include"
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
)
message(STATUS "Windows target version: ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")

ExternalProject_Get_property(CppWinRTNuget SOURCE_DIR)
ExternalProject_Get_property(CppWinRTNuget BINARY_DIR)

add_library(CppWinRT INTERFACE)
add_dependencies(CppWinRT CppWinRTNuget)
target_link_libraries(CppWinRT INTERFACE System::WindowsApp)
target_include_directories(CppWinRT INTERFACE "${BINARY_DIR}/include")
add_library(ThirdParty::CppWinRT ALIAS CppWinRT)

install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-CppWinRT.txt"
)
