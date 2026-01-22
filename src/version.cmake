if ("$ENV{GITHUB_REF_TYPE}" STREQUAL "tag")
  set(MATCHING_TAG $ENV{GITHUB_REF_NAME})
else ()
  set(MATCHING_TAG NOTFOUND)
endif ()

if (DEFINED ENV{GITHUB_RUN_NUMBER})
  set(IS_GITHUB_ACTIONS_BUILD "true")
  set(BUILD_TYPE "gha")
else ()
  set(IS_GITHUB_ACTIONS_BUILD "false")
  set(BUILD_TYPE "local")
endif ()

set(IS_TAGGED_VERSION false)

if (MATCHING_TAG)
  set(RELEASE_NAME "${MATCHING_TAG}+${BUILD_TYPE}.${VERSION_BUILD}")
  set(IS_TAGGED_VERSION true)
elseif (IS_GITHUB_ACTIONS_BUILD)
  set(RELEASE_NAME "v${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}+${BUILD_TYPE}.${VERSION_BUILD}")
else ()
  set(RELEASE_NAME "v${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}+${BUILD_TYPE}")
endif ()

math(EXPR IS_DEVELOPMENT_BRANCH "${VERSION_MINOR} % 2")
if (IS_DEVELOPMENT_BRANCH)
  message(DEBUG "${RELEASE_NAME} is on a development branch.")
else ()
  message(DEBUG "${RELEASE_NAME} is on a stable branch.")
endif ()

if (
  IS_TAGGED_VERSION STREQUAL "true"
  AND IS_GITHUB_ACTIONS_BUILD
  AND NOT "${RELEASE_NAME}" MATCHES "-"
  AND NOT IS_DEVELOPMENT_BRANCH)
  set(IS_STABLE_RELEASE true)
else ()
  set(IS_STABLE_RELEASE false)
endif ()
message(DEBUG "Stable release: ${IS_STABLE_RELEASE}")

if (INPUT_CPP_FILE)
  configure_file(
    ${INPUT_CPP_FILE}
    ${OUTPUT_CPP_FILE}
    @ONLY
  )
endif ()

if (INPUT_RC_FILE)
  configure_file(
    ${INPUT_RC_FILE}
    ${OUTPUT_RC_FILE}
    @ONLY
  )
endif ()
