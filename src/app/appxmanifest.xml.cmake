if(DEFINED ENV{GITHUB_RUN_ID})
  set(VERSION_BUILD $ENV{GITHUB_RUN_ID})
else()
  execute_process(
    COMMAND git rev-list --count HEAD
    OUTPUT_VARIABLE VERSION_BUILD
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
endif()

configure_file(
  ${INPUT_FILE}
  ${OUTPUT_FILE}
  @ONLY
)
