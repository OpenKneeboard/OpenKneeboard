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

if("${MODIFIED_FILES}" STREQUAL "")
  set(DIRTY "false")
else()
  set(DIRTY "true")
endif()

configure_file(
  ${INPUT_FILE}
  ${OUTPUT_FILE}
  @ONLY
)
