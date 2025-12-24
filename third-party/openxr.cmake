include(ExternalProject)
ExternalProject_Add(
  OpenXRSDKSource
  URL "https://github.com/KhronosGroup/OpenXR-SDK/archive/refs/tags/release-1.0.25.zip"
  URL_HASH "SHA256=a4d3b8877b2ab1fd0ccddeced8e5f7fe7940cf48793bcc29fb0611c6a53af65e"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  STEP_TARGETS update
)

ExternalProject_Get_property(OpenXRSDKSource SOURCE_DIR)

add_library(OpenXRSDK INTERFACE)
add_dependencies(OpenXRSDK OpenXRSDKSource)
target_include_directories(
  OpenXRSDK
  INTERFACE
  "${SOURCE_DIR}/src/common"
  "${SOURCE_DIR}/include"
)

add_library(ThirdParty::OpenXR ALIAS OpenXRSDK)

include(ok_add_license_file)
ok_add_license_file(
  "${SOURCE_DIR}/LICENSE"
  "LICENSE-ThirdParty-OpenXR SDK.txt"
  DEPENDS OpenXRSDKSource-update
)
