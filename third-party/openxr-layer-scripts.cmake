ExternalProject_Add(
  openxrLayerScriptsEP
  URL "https://github.com/fredemmott/openxr-layer-scripts/archive/refs/tags/v1.1.0.zip"
  URL_HASH "SHA256=fc09e9e703d162401e9f67f46efd1b8db494ad5d9730dad35b840171ffcddfc9"
  BUILD_BYPRODUCTS
  "<SOURCE_DIR>/edit-openxr-layers.ps1"
  "<SOURCE_DIR>/list-openxr-layers.ps1"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_IN_SOURCE ON
)

ExternalProject_Get_property(openxrLayerScriptsEP SOURCE_DIR)

message(STATUS "scripts ${SOURCE_DIR}")

add_signed_script(
  edit-openxr-layers
  "${SOURCE_DIR}/edit-openxr-layers.ps1"
  DESTINATION scripts/
)
add_signed_script(
  list-openxr-layers
  "${SOURCE_DIR}/list-openxr-layers.ps1"
  DESTINATION scripts/
)

install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-OpenXR-Layer-Scripts.txt"
)
