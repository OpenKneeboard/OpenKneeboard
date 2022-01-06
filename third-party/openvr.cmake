ExternalProject_Add(
  openvrBuild
  URL "https://github.com/ValveSoftware/openvr/archive/refs/tags/v1.16.8.zip"
  URL_HASH "SHA256=84b99bbdfe00898d74d15d3e127d9236484f581304fff9ebc5317f4700ab1cee"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_IN_SOURCE ON
  EXCLUDE_FROM_ALL
)

ExternalProject_Get_property(openvrBuild SOURCE_DIR)

add_library(openvr SHARED IMPORTED GLOBAL)
add_dependencies(openvr openvrBuild)

target_include_directories(openvr INTERFACE "${SOURCE_DIR}/headers")
set_target_properties(
  openvr
  PROPERTIES
  IMPORTED_IMPLIB "${SOURCE_DIR}/lib/win64/openvr_api.lib"
  IMPORTED_LOCATION "${SOURCE_DIR}/bin/win64/openvr_api.dll")
