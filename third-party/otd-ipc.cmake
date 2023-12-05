ExternalProject_Add(
  OTDIPCFetch
  URL "https://github.com/OpenKneeboard/OTD-IPC/archive/refs/tags/v0.1.tar.gz"
  URL_HASH "SHA256=22ea9598b12b4dcca3bfdba2775587250170df8f8e95ba24735dd367819acc74"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

ExternalProject_Get_property(OTDIPCFetch SOURCE_DIR)

add_library(OTDIPC INTERFACE)
add_dependencies(OTDIPC OTDIPCFetch)
target_include_directories(
  OTDIPC
  INTERFACE
  "${SOURCE_DIR}/include"
)

add_library(ThirdParty::OTDIPC ALIAS OTDIPC)

install(
  FILES "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-OpenKneeboard-OTD-IPC.txt"
)
