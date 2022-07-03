set(BUILD_SHARED_LIBS OFF)

set(
  THIRDPARTY_TOOLCHAIN_FILE
  "${CMAKE_CURRENT_BINARY_DIR}/toolchain.cmake"
)
configure_file(
  toolchain.cmake.in
  "${THIRDPARTY_TOOLCHAIN_FILE}"
  @ONLY
)

include(detours.cmake)
include(cppwinrt.cmake)
include(directxtk.cmake)
include(json.cmake)
include(libjpeg-turbo.cmake)
include(libzip.cmake)
include(lualib.cmake)
include(LuaBridge.cmake) # must be after lualib
include(oculus-sdk.cmake)
include(openvr.cmake)
include(openxr.cmake)
include(qpdf.cmake)
include(system.cmake)
include(wintab.cmake)
include(zlib.cmake)
