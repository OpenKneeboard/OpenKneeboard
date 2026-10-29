vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO juj/RectangleBinPack
  REF 83e7e1132d93777e3732dfaae26b0f3703be2036
  SHA512 018388182f205133c4bfdb6a49c2fa0c07ebcaccc7491ea54a6700cdecaf2fdeb8ce7eb471243d39247dbc463b5cb8c71e27216d18b57dcb25e82dea80098947
  HEAD_REF master
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_build_cmake(TARGET RectangleBinPack)

vcpkg_copy_pdbs()

file(GLOB HEADER_FILES "${SOURCE_PATH}/*.h")
file(
  INSTALL ${HEADER_FILES}
  DESTINATION "${CURRENT_PACKAGES_DIR}/include/RectangleBinPack"
  FILES_MATCHING PATTERN "*.h"
)

if (NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "release")
  file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/RectangleBinPack.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
endif ()
if (NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "debug")
  file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-dbg/RectangleBinPack.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib")
endif ()

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

include(CMakePackageConfigHelpers)
macro(make_package_config_file PREFIX)
  configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/${PORT}-config.cmake.in"
    "${CURRENT_PACKAGES_DIR}/${PREFIX}/${PORT}/${PORT}-config.cmake"
    INSTALL_DESTINATION "${PREFIX}/${PORT}"
  )
endmacro()
make_package_config_file("share")
make_package_config_file("debug/share")
vcpkg_cmake_config_fixup(PACKAGE_NAME "${PORT}")