set(LIB "lib/win64/openvr_api.lib")
ExternalProject_Add(
  openvrBuild
  URL "https://github.com/ValveSoftware/openvr/archive/refs/tags/v1.16.8.zip"
  URL_HASH "SHA256=84b99bbdfe00898d74d15d3e127d9236484f581304fff9ebc5317f4700ab1cee"
  BUILD_BYPRODUCTS "<SOURCE_DIR>/${LIB}"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_IN_SOURCE ON
  EXCLUDE_FROM_ALL
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
  IMPORTED_IMPLIB "${SOURCE_DIR}/${LIB}"
  IMPORTED_LOCATION "${SOURCE_DIR}/bin/win64/openvr_api.dll")
target_link_libraries(openvr INTERFACE openvr-headers)

ExternalProject_Get_property(openvrBuild SOURCE_DIR)
install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-OpenVR.txt"
)

add_library(ThirdParty::OpenVR ALIAS openvr)
