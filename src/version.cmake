execute_process(
  COMMAND git describe --tags --abbrev=0 HEAD
  OUTPUT_VARIABLE LATEST_GIT_TAG
  OUTPUT_STRIP_TRAILING_WHITESPACE
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if("${VERSION_PATCH}" STREQUAL "0")
  set(MATCHING_TAG OFF)
else()
  string(REGEX MATCH "^v${VERSION_MAJOR}\\.${VERSION_MINOR}\\.${VERSION_PATCH}(\\.|-|$)" MATCHING_TAG "${LATEST_GIT_TAG}")
endif()

if(DEFINED ENV{GITHUB_RUN_NUMBER})
  set(IS_GITHUB_ACTIONS_BUILD "true")
  set(BUILD_TYPE "gha")
else()
  set(IS_GITHUB_ACTIONS_BUILD "false")
  set(BUILD_TYPE "local")
endif()

if("${RELEASE_NAME}" STREQUAL "")
  if(MATCHING_TAG)
    set(RELEASE_NAME "${LATEST_GIT_TAG}+${BUILD_TYPE}.rev.${VERSION_BUILD}")
  else()
    set(RELEASE_NAME "v${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}+${BUILD_TYPE}")
  endif()
endif()

if(INPUT_CPP_FILE)
  configure_file(
    ${INPUT_CPP_FILE}
    ${OUTPUT_CPP_FILE}
    @ONLY
  )
endif ()

if(INPUT_RC_FILE)
  configure_file(
    ${INPUT_RC_FILE}
    ${OUTPUT_RC_FILE}
    @ONLY
  )
endif()
