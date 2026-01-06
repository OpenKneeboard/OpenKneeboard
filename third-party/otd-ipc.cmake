ExternalProject_Add(
  OTDIPCFetch
  GIT_REPOSITORY "https://github.com/OpenKneeboard/OTD-IPC.git"
  GIT_TAG "bd35fcec030b9bfb5fb91f2d851fa4fad11d25b4"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  STEP_TARGETS update
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

include(ok_add_license_file)
ok_add_license_file(
  "${SOURCE_DIR}/LICENSE"
  "LICENSE-ThirdParty-OpenKneeboard-OTD-IPC.txt"
  DEPENDS OTDIPCFetch-update
)
