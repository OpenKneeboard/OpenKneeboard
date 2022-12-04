
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
    -DVERSION_BUILD=${CMAKE_PROJECT_VERSION_TWEAK}
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
