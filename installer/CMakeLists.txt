install(
  FILES
  "WiX-LICENSE.txt"
  TYPE DOC
  RENAME "LICENSE-WiX.txt"
)

##### CPack (MSI) Config #####
configure_file("${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_CURRENT_BINARY_DIR}/license.wix.txt" COPYONLY)

set(CPACK_GENERATOR "WIX")
set(CPACK_PACKAGE_NAME "OpenKneeboard")
set(CPACK_PACKAGE_VENDOR "Fred Emmott")
set(CPACK_WIX_UPGRADE_GUID "843c9331-0610-4ab1-9cf9-5305c896fb5b")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_BINARY_DIR}/license.wix.txt")
set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/assets/icon.ico")
set(CPACK_WIX_COMPONENT_INSTALL ON)
set(CPACK_WIX_UI_REF "WixUI_Minimal_NoEULA")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")

get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL DebugSymbols)

list(
  APPEND
  CPACK_WIX_EXTRA_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenXRAPILayerRegistration.wxs"
  "${CMAKE_CURRENT_SOURCE_DIR}/WixUI_Minimal_NoEULA.wxs"
)
list(
  APPEND
  CPACK_WIX_PATCH_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/patch.xml"
)

include(CPack)
cpack_add_component(
  Default
  HIDDEN
)
cpack_add_component(
  Utilities
  HIDDEN
)
