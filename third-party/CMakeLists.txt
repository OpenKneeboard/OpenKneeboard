set(BUILD_SHARED_LIBS OFF)

set(THIRD_PARTY_COMPILE_OPTIONS ${COMMON_COMPILE_OPTIONS})
if (MSVC)
  list(APPEND THIRD_PARTY_COMPILE_OPTIONS "/WX-")
else ()
  list(APPEND THIRD_PARTY_COMPILE_OPTIONS "-Wno-error")
endif ()

set(
  THIRDPARTY_TOOLCHAIN_FILE
  "${CMAKE_CURRENT_BINARY_DIR}/toolchain.cmake"
)
configure_file(
  toolchain.cmake.in
  "${THIRDPARTY_TOOLCHAIN_FILE}"
  @ONLY
)

set(OKB_VCPKG_SHARE_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share")

scoped_include(bindline.cmake)
scoped_include(boost.cmake)
scoped_include(cef.cmake)
scoped_include(cppwinrt.cmake)
scoped_include(detours.cmake)
scoped_include(dia.cmake)
scoped_include(directxtk.cmake)
scoped_include(directxtk12.cmake)
scoped_include(dxc.cmake)
scoped_include(felly.cmake)
scoped_include(geographiclib.cmake)
scoped_include(json.cmake)
scoped_include(libzip.cmake)
scoped_include(lualib.cmake)
scoped_include(magic_enum.cmake)
scoped_include(magic_json_serialize_enum.cmake)
scoped_include(openvr.cmake)
scoped_include(openxr.cmake)
scoped_include(otd-ipc.cmake)
scoped_include(qpdf.cmake)
scoped_include(renderdoc.cmake)
scoped_include(semver.cmake)
scoped_include(system.cmake)
scoped_include(vulkan-headers.cmake)
scoped_include(wil.cmake)
scoped_include(windowsappsdk.cmake)
scoped_include(wmm.cmake)

include(ok_add_license_file)
file(GLOB COPYRIGHT_FILES "${OKB_VCPKG_SHARE_DIR}/*/copyright")
foreach (FILE IN LISTS COPYRIGHT_FILES)
  cmake_path(GET FILE PARENT_PATH PACKAGE_FOLDER)
  cmake_path(GET PACKAGE_FOLDER FILENAME PACKAGE_NAME)
  ok_add_license_file("${FILE}" "LICENSE-ThirdParty-${PACKAGE_NAME}.txt")
endforeach ()