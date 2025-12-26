include_guard(GLOBAL)

find_path(
  OPENVR_HEADER_PATH
  NAMES "openvr_capi.h"
)
if (NOT OPENVR_HEADER_PATH)
  return()
endif ()
find_library(OPENVR_LIB NAMES "openvr_api" REQUIRED)
get_filename_component(LIB_DIR "${OPENVR_LIB}" DIRECTORY)
find_file(
  OPENVR_DLL
  NAMES
  "openvr_api.dll"
  HINTS
  "${LIB_DIR}/../bin"
  REQUIRED
)

add_library(ThirdParty::OpenVR SHARED IMPORTED GLOBAL)
set_target_properties(
  ThirdParty::OpenVR
  PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${OPENVR_HEADER_PATH}"
  IMPORTED_IMPLIB "${OPENVR_LIB}"
  IMPORTED_LOCATION "${OPENVR_DLL}"
  INTERFACE_LINK_OPTIONS "/DELAYLOAD:openvr_api.dll"
)

include(ok_add_runtime_files)
ok_add_runtime_files(copy-openvr-dll "${OPENVR_DLL}")