include(ExternalProject)

# Used for nuget
set(CPPWINRT_VERSION "2.0.230706.1" CACHE INTERNAL "")
ExternalProject_Add(
  CppWinRTNuget
  URL "https://www.nuget.org/api/v2/package/Microsoft.Windows.CppWinRT/2.0.230706.1"
  URL_HASH "SHA256=a99eca1c244dd730b31554e6d4850e685f40bfb7cb0bd1cfb1561169fc3b692b"

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
target_include_directories(CppWinRT INTERFACE "${BINARY_DIR}/include")
add_library(ThirdParty::CppWinRT ALIAS CppWinRT)

add_executable(ThirdParty::CppWinRT::Exe IMPORTED GLOBAL)
set_target_properties(
  ThirdParty::CppWinRT::Exe
  PROPERTIES
  IMPORTED_LOCATION
  "${SOURCE_DIR}/bin/cppwinrt.exe"
)

install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-CppWinRT.txt"
)
