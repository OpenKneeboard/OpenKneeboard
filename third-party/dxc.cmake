# DXC is included in the windows SDK, but the version has SPIR-V (Vulkan support) disabled
include(ExternalProject)
ExternalProject_Add(
  dxcEP
  URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.7.2308/dxc_2023_08_14.zip"
  URL_HASH "SHA256=01d4c4dfa37dee21afe70cac510d63001b6b611a128e3760f168765eead1e625"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

add_executable(ThirdParty::dxc IMPORTED GLOBAL)
add_dependencies(ThirdParty::dxc dxcEP)

ExternalProject_Get_property(dxcEP SOURCE_DIR)
set_target_properties(
    ThirdParty::dxc
    PROPERTIES
    IMPORTED_LOCATION "${SOURCE_DIR}/bin/x64/dxc.exe"
)