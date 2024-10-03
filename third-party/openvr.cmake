include(ExternalProject)
include(ok_add_runtime_files)

set(LIB_PATH "lib/win64/openvr_api.lib")
set(DLL_PATH "bin/win64/openvr_api.dll")

ExternalProject_Add(
  openvrBuild
  URL "https://github.com/ValveSoftware/openvr/archive/refs/tags/v1.23.7.zip"
  URL_HASH "SHA256=7ffc01fcda5914cdba555074aa334c2c30c4b07c576d651460420f26d9fb7c6a"
  BUILD_BYPRODUCTS "<SOURCE_DIR>/${LIB_PATH}" "<SOURCE_DIR>/${DLL_PATH}"
  PATCH_COMMAND

  # Stop IDEs (VSCode) from picking up the outdated Vulkan headers here
  "${CMAKE_COMMAND}" -E rm -rf "<SOURCE_DIR>/samples"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_IN_SOURCE ON

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

ExternalProject_Get_property(openvrBuild SOURCE_DIR)
file(MAKE_DIRECTORY "${SOURCE_DIR}/headers")

add_library(openvr-headers INTERFACE)
add_dependencies(openvr-headers openvrBuild)
target_include_directories(openvr-headers INTERFACE "${SOURCE_DIR}/headers")

add_library(openvr SHARED IMPORTED GLOBAL)
add_dependencies(openvr openvrBuild)

set_target_properties(
  openvr
  PROPERTIES
  IMPORTED_IMPLIB "${SOURCE_DIR}/${LIB_PATH}"
  IMPORTED_LOCATION "${SOURCE_DIR}/${DLL_PATH}"
)
target_link_libraries(openvr INTERFACE openvr-headers)

ExternalProject_Get_property(openvrBuild SOURCE_DIR)
ok_add_runtime_files(copy-openvr-dll "${SOURCE_DIR}/${DLL_PATH}")
install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-OpenVR SDK.txt"
)

add_library(ThirdParty::OpenVR ALIAS openvr)