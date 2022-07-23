
add_compile_options(
  "/Zc:__cplusplus"
  "/utf-8"
  # All warnings as errors
  "/WX"
  # Lots of legacy C functions are unsafe, and raise warnings in third-party code
  "/wd4996"
  # Informational 'warning' describing how diamond inheritance works
  "/wd4250"
)

add_compile_definitions(
  "UNICODE=1"
  "_UNICODE=1"
  "NOMINMAX=1"
  "WIN32_LEAN_AND_MEAN=1"
  "$<IF:$<CONFIG:Debug>,DEBUG,NDEBUG>"
  "OPENKNEEBOARD_BREAK=$<$<CONFIG:Debug>:__debugbreak()>"
  "BUILD_CONFIG=\"$<CONFIG>\""
)

# Always link this so we never accidentally use the SDK version
link_libraries(ThirdParty::CppWinRT)

set(GITHUB_REF_TYPE "" CACHE STRING "For GitHub Actions: 'tag' or 'branch'")
set(GITHUB_REF_NAME "" CACHE STRING "For GitHub Actions: the name of the branch or tag being built")
if(GITHUB_REF_TYPE STREQUAL "tag")
  set(DEFAULT_RELEASE_NAME "${GITHUB_REF_NAME}")
else()
  set(DEFAULT_RELEASE_NAME "")
endif()

set(RELEASE_NAME "${DEFAULT_RELEASE_NAME}" CACHE STRING "Human-readable version to show to users")

function(create_version_rc RC_TARGET ORIGINAL_TARGET ORIGINAL_FILENAME OUTPUT_RC_FILE)
  add_custom_target(
    "${RC_TARGET}"
    COMMAND
    ${CMAKE_COMMAND}
    -DVERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
    -DVERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
    -DVERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
    "-DACTIVE_BUILD_MODE=$<CONFIG>"
    "-DINPUT_RC_FILE=${CMAKE_SOURCE_DIR}/src/version.rc.in"
    "-DOUTPUT_RC_FILE=${OUTPUT_RC_FILE}"
    "-DFILE_DESCRIPTION=${ORIGINAL_TARGET}"
    "-DORIGINAL_FILENAME=${ORIGINAL_FILENAME}"
    "-DRELEASE_NAME=${RELEASE_NAME}"
    "-P${CMAKE_SOURCE_DIR}/src/version.cmake"
    BYPRODUCTS "${OUTPUT_RC_FILE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  )
endfunction()

function(add_version_rc TARGET ORIGINAL_FILENAME)
  set(VERSION_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.version.rc")
  create_version_rc(
    "${TARGET}-version-rc"
    "${TARGET}"
    "${ORIGINAL_FILENAME}"
    "${VERSION_RC_FILE}"
  )
  target_sources("${TARGET}" PRIVATE "${VERSION_RC_FILE}")
endfunction()

function(add_module_library TARGET)
  add_library(${TARGET} MODULE ${ARGN})
  add_version_rc(${TARGET} ${TARGET}.dll)
  sign_target(${TARGET})
endfunction()

add_subdirectory(lib)

add_subdirectory(dcs-hook)
add_subdirectory(injectables)
add_subdirectory(games)

add_subdirectory(utilities)
add_subdirectory(app)

##### CPack (MSI) Config #####

configure_file("${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_CURRENT_BINARY_DIR}/license.wix.txt" COPYONLY)

set(CPACK_GENERATOR "WIX")
set(CPACK_PACKAGE_NAME "OpenKneeboard")
set(CPACK_PACKAGE_VENDOR "Fred Emmott")
set(CPACK_WIX_UPGRADE_GUID "843c9331-0610-4ab1-9cf9-5305c896fb5b")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_BINARY_DIR}/license.wix.txt")
set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/assets/icon.ico")
set(CPACK_WIX_COMPONENT_INSTALL ON)
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")

get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL DebugSymbols)

include(CPack)
cpack_add_component(
  Default
  HIDDEN
)
cpack_add_component(
  Utilities
  DISPLAY_NAME "Utilities"
  DESCRIPTION "Utilities to control OpenKneeboard (e.g. change tab) from other scripts or programs."
  DISABLED
)
