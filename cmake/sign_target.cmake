include_guard(GLOBAL)
include(windows_kits_dir)

set(SIGNTOOL_KEY_FILE "" CACHE STRING "File containing code-signing key")
find_program(
  SIGNTOOL_EXE
  signtool
  PATHS
  "${WINDOWS_10_KIT_DIR}/x64"
  "${WINDOWS_10_KIT_DIR}/x86"
  DOC "Path to signtool.exe if SIGNTOOL_KEY_FILE is set"
)

function(sign_target_file TARGET FILE)
  if(SIGNTOOL_KEY_FILE)
    add_custom_command(
      TARGET ${TARGET} POST_BUILD
      COMMAND
      "${SIGNTOOL_EXE}"
      ARGS
      sign
      /f
      "${SIGNTOOL_KEY_FILE}"
      /t http://timestamp.digicert.com
      /fd SHA256
      "${FILE}"
    )
  endif()
endfunction()

function(sign_target TARGET)
  sign_target_file("${TARGET}" "$<TARGET_FILE:${TARGET}>")
endfunction()