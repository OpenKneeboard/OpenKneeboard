ExternalProject_Add(
  OpenXRSDKSource
  URL "https://github.com/KhronosGroup/OpenXR-SDK/archive/refs/tags/release-1.0.22.zip"
  URL_HASH "SHA256=f0bfc275754f486f995845898002db2bf3225bff563782f66e86906183c702c3"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  EXCLUDE_FROM_ALL
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
