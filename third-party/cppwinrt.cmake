include(ExternalProject)

ExternalProject_Add(
  CppWinRTNuget
  URL "https://github.com/microsoft/cppwinrt/releases/download/2.0.220608.4/Microsoft.Windows.CppWinRT.2.0.220608.4.nupkg"
  URL_HASH "SHA256=418836f3ca50b5e9d2213860467986ed4e78919eb6744da5bd772850a2b3320a"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND
    "<SOURCE_DIR>/bin/cppwinrt.exe"
    -in "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}"
    -output "<BINARY_DIR>/include"
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
)

ExternalProject_Get_property(CppWinRTNuget SOURCE_DIR)
ExternalProject_Get_property(CppWinRTNuget BINARY_DIR)

add_library(ThirdParty::CppWinRT INTERFACE IMPORTED GLOBAL)
add_dependencies(ThirdParty::CppWinRT CppWinRTNuget)
target_link_libraries(ThirdParty::CppWinRT INTERFACE System::WindowsApp)
target_include_directories(ThirdParty::CppWinRT INTERFACE "${BINARY_DIR}/include")

install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-cppwinrt.txt"
)
