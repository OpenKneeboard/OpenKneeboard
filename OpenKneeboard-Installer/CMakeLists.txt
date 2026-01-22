if (NOT BUILD_IS_64BIT)
  return()
endif ()

set(VERSION_JSON "${BUILD_OUT_ROOT}/installer/version.json")
message(STATUS "Creating ${VERSION_JSON}")
add_custom_target(
  installer-version-json
  ALL
  COMMAND
  "${CMAKE_COMMAND}"
  -DVERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
  -DVERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
  -DVERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
  -DVERSION_BUILD=${CMAKE_PROJECT_VERSION_TWEAK}
  "-DACTIVE_BUILD_MODE=$<CONFIG>"
  "-DINPUT_RC_FILE=${CMAKE_CURRENT_SOURCE_DIR}/version.json.in"
  "-DOUTPUT_RC_FILE=${VERSION_JSON}"
  "-P${CMAKE_SOURCE_DIR}/src/version.cmake"
  DEPENDS
  "${CMAKE_SOURCE_DIR}/src/version.cmake"
  "${CMAKE_CURRENT_LIST_FILE}"
  "version.json.in"
  BYPRODUCTS
  "${VERSION_JSON}"
)

ok_add_executable(remove-openkneeboard-msix WIN32 remove-msix.cpp)
target_link_libraries(remove-openkneeboard-msix PRIVATE ThirdParty::CppWinRT)
set_target_properties(
  remove-openkneeboard-msix
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${BUILD_OUT_ROOT}/installer"
)

add_custom_target(
  installer-assets
  ALL
  COMMAND
  "${CMAKE_COMMAND}"
  -E make_directory
  "${BUILD_OUT_ROOT}/installer/assets"
  COMMAND
  "${CMAKE_COMMAND}"
  -E copy_if_different
  "${CMAKE_SOURCE_DIR}/assets/icon.ico"
  "${CMAKE_SOURCE_DIR}/assets/WixUIBanner.png"
  "${CMAKE_SOURCE_DIR}/assets/WixUIDialog.png"
  "${BUILD_OUT_ROOT}/installer/assets"
  COMMAND
  "${CMAKE_COMMAND}"
  -E copy_if_different
  "${CMAKE_CURRENT_SOURCE_DIR}/OpenKneeboardPluginFileType.wxs"
  "${CMAKE_CURRENT_SOURCE_DIR}/WixUI_Minimal_NoEULA.wxs"
  "${BUILD_OUT_ROOT}/installer/"
  DEPENDS remove-openkneeboard-msix
)
