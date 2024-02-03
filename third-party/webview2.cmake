include(ExternalProject)

ExternalProject_Add(
  WebView2EP
  URL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2210.55"
  URL_HASH "SHA256=c0945ddfe38272f10f24673a1cb07b54b018f614eec85d3e830628369039ad3d"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND
    ThirdParty::CppWinRT::Exe
    -input "<SOURCE_DIR>/lib/Microsoft.Web.WebView2.Core.winmd"
    -output "<BINARY_DIR>/cppwinrt/include"
    -reference "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}+"
  COMMAND
    ThirdParty::AbiWinRT
    -input "<SOURCE_DIR>/lib/Microsoft.Web.WebView2.Core.winmd"
    -output "<BINARY_DIR>/abi/include"
    -reference "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}+"
    -ns-prefix
    -lowercase-include-guard
    -enable-header-deprecation
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

ExternalProject_Get_property(WebView2EP SOURCE_DIR)
ExternalProject_Get_property(WebView2EP BINARY_DIR)

add_library(WebView2Loader INTERFACE)
add_dependencies(WebView2Loader WebView2EP)
target_include_directories(
  WebView2Loader
  INTERFACE
  "${SOURCE_DIR}/build/native/include"
  "${BINARY_DIR}/abi/include"
  "${BINARY_DIR}/cppwinrt/include"
)
target_link_libraries(
  WebView2Loader
  INTERFACE
  "${SOURCE_DIR}/build/native/x64/WebView2LoaderStatic.lib"
)
add_library(ThirdParty::WebView2Loader ALIAS WebView2Loader)

install(
  FILES
  "${SOURCE_DIR}/LICENSE.txt"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-WebView2.txt"
)