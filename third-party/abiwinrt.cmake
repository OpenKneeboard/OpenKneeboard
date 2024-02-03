include(ExternalProject)

ExternalProject_Add(
  abiwinrtEP
  URL "https://www.nuget.org/api/v2/package/Microsoft.Windows.AbiWinRT/2.0.210330.2"
  URL_HASH "SHA256=9f94ddbd2fe85ee9e71787c6b67eec03939b2017d6bfab05a064b07b51f99d66"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

ExternalProject_Get_property(abiwinrtEP SOURCE_DIR)

add_executable(ThirdParty::AbiWinRT IMPORTED)
add_dependencies(ThirdParty::AbiWinRT abiwinrtEP)
set_target_properties(
  ThirdParty::AbiWinRT 
  PROPERTIES
  IMPORTED_LOCATION "${SOURCE_DIR}/bin/abi.exe"
)

install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-AbiWinRT.txt"
)