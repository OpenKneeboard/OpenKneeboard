vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO fredemmott/felly
  REF "2e9a9bc5dfd726783784354207d5175ade03feb1"
  SHA512 ddc97df27cecf265b2f6c4a296fab60214c0f686f9186e6dcbedd6147ddb0d2e4e7b70485a8f47274ce91f37b222db453292d9ebbe649b786976d3ff08786cf7
  HEAD_REF master
)

file(
  INSTALL "${SOURCE_PATH}/include/"
  DESTINATION "${CURRENT_PACKAGES_DIR}/include"
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

include(CMakePackageConfigHelpers)
macro(make_package_config_file PREFIX)
  configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/felly-config.cmake.in"
    "${CURRENT_PACKAGES_DIR}/${PREFIX}/${PORT}/${PORT}-config.cmake"
    INSTALL_DESTINATION "${PREFIX}/${PORT}"
  )
endmacro()
make_package_config_file("share")
make_package_config_file("debug/share")
vcpkg_cmake_config_fixup(PACKAGE_NAME "${PORT}")