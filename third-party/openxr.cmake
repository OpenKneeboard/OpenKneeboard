ExternalProject_Add(
  OpenXRSDKSource
  URL "https://github.com/KhronosGroup/OpenXR-SDK/archive/refs/tags/release-1.0.25.zip"
  URL_HASH "SHA256=a4d3b8877b2ab1fd0ccddeced8e5f7fe7940cf48793bcc29fb0611c6a53af65e"
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

install(
	FILES "${SOURCE_DIR}/LICENSE"
	TYPE DOC
	RENAME "LICENSE-ThirdParty-OpenXR SDK.txt"
)
