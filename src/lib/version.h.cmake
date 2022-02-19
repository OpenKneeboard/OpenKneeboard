execute_process(
  COMMAND git rev-parse HEAD
  OUTPUT_VARIABLE COMMIT_ID
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND git status --short
  OUTPUT_VARIABLE MODIFIED_FILES
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(
  REPLACE
  "\n"
  "\\n\\\n"
  MODIFIED_FILES
  "${MODIFIED_FILES}"
)

string(TIMESTAMP BUILD_TIMESTAMP UTC)

execute_process(
  COMMAND git log -1 "--format=%at"
  OUTPUT_VARIABLE COMMIT_UNIX_TIMESTAMP
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if("${MODIFIED_FILES}" STREQUAL "")
  set(DIRTY "false")
else()
  set(DIRTY "true")
endif()

if(DEFINED ENV{GITHUB_RUN_ID})
  set(VERSION_BUILD $ENV{GITHUB_RUN_ID})
  set(IS_GITHUB_ACTIONS_BUILD "true")
else()
  execute_process(
    COMMAND git rev-list --count HEAD
    OUTPUT_VARIABLE VERSION_BUILD
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  set(IS_GITHUB_ACTIONS_BUILD "false")
endif()

configure_file(
  ${INPUT_FILE}
  ${OUTPUT_FILE}
  @ONLY
)
