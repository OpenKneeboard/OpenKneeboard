set(BUILD_SHARED_LIBS OFF)

list(
  APPEND
  CMAKE_VS_GLOBALS
  "ClangTidyToolPath=${CMAKE_CURRENT_SOURCE_DIR}"
  "ClangTidyToolExe=clang-tidy-stub.bat"
)

# For targets with no C/C++
macro(disable_clang_tidy TARGET)
  set_target_properties(
    "${TARGET}"
    PROPERTIES
    VS_GLOBAL_ClangTidyToolPath "${CMAKE_SOURCE_DIR}/third-party"
    VS_GLOBAL_ClangTidyToolExe clang-tidy-stub.bat
  )
endmacro()

set(THIRD_PARTY_COMPILE_OPTIONS ${COMMON_COMPILE_OPTIONS})
if(MSVC)
  list(APPEND THIRD_PARTY_COMPILE_OPTIONS "/WX-")
else()
  list(APPEND THID_PARTY_COMPILE_OPTIONS "-Wno-error")
endif()

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
include(openvr.cmake)
include(otd-ipc.cmake)
include(openxr.cmake)
include(renderdoc.cmake)
include(semver.cmake)
include(system.cmake)
include(vulkan-headers.cmake)
include(wil.cmake)
include(wintab.cmake)
include(wmm.cmake)
include(zlib.cmake)

include(libzip.cmake)
include(weak_refs.cmake)
include(webview2.cmake)
include(windowsappsdk.cmake)
include(qpdf.cmake)
