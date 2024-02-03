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

include(abiwinrt.cmake)
include(detours.cmake)
include(cppwinrt.cmake)
include(directxtex.cmake)
include(directxtk.cmake)
include(directxtk12.cmake)
include(dxc.cmake)
include(geographiclib.cmake)
include(json.cmake)
include(libjpeg-turbo.cmake)
include(lualib.cmake)
include(oculus-sdk.cmake)
include(out_ptr.cmake)
include(openvr.cmake)
include(otd-ipc.cmake)
include(openxr.cmake)
include(renderdoc.cmake)
include(semver.cmake)
include(system.cmake)
include(vulkan-headers.cmake)
include(wintab.cmake)
include(wmm.cmake)
include(zlib.cmake)

include(libzip.cmake)
include(webview2.cmake)
include(windowsappsdk.cmake)
include(qpdf.cmake)
