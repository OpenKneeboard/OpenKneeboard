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
  "BUILD_CONFIG=\"$<CONFIG>\""
)

# TODO (C++23): replace all calls with std::unreachable()
add_compile_definitions(
  "OPENKNEEBOARD_UNREACHABLE=__assume(false)"
)

add_subdirectory(shaders)

# Always link this so we never accidentally use the SDK version
link_libraries(ThirdParty::CppWinRT)

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
    "-P${CMAKE_SOURCE_DIR}/src/version.cmake"
    DEPENDS
    "${CMAKE_SOURCE_DIR}/src/version.cmake"
    BYPRODUCTS "${OUTPUT_RC_FILE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  disable_clang_tidy("${RC_TARGET}")
endfunction()

function(add_version_rc TARGET)
  set(VERSION_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.version.rc")
  create_version_rc(
    "${TARGET}-version-rc"
    "$<TARGET_FILE_BASE_NAME:${TARGET}>"
    "$<TARGET_FILE_NAME:${TARGET}>"
    "${VERSION_RC_FILE}"
  )
  target_sources("${TARGET}" PRIVATE "${VERSION_RC_FILE}")
endfunction()

function(ok_postprocess_target TARGET)
  get_target_property(ALIAS "${TARGET}" ALIASED_TARGET)

  if(ALIAS)
    return()
  endif()

  get_target_property(TARGET_TYPE "${TARGET}" TYPE)

  if(NOT TARGET_TYPE MATCHES "^(EXECUTABLE|(SHARED|MODULE)_LIBRARY)$")
    return()
  endif()

  add_version_rc(${TARGET})
  sign_target(${TARGET})
  install(FILES "$<TARGET_PDB_FILE:${TARGET}>" DESTINATION "." COMPONENT DebugSymbols)
endfunction()

function(ok_add_library TARGET)
  add_library("${TARGET}" ${ARGN})
  ok_postprocess_target("${TARGET}")
endfunction()

function(ok_add_executable TARGET)
  add_executable("${TARGET}" ${ARGN})
  ok_postprocess_target("${TARGET}")
endfunction()

function(add_module_library TARGET)
  ok_add_library(${TARGET} MODULE ${ARGN})
endfunction()

add_subdirectory(lib)
add_subdirectory(api)

add_subdirectory(dcs-hook)
add_subdirectory(injectables)
add_subdirectory(games)

add_subdirectory(utilities)
add_subdirectory(app)