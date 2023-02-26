get_filename_component(
  WINDOWS_10_KITS_ROOT
  "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]"
  ABSOLUTE CACHE
)
set(WINDOWS_10_KIT_DIR "${WINDOWS_10_KITS_ROOT}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}" CACHE PATH "Current Windows 10 kit directory")
set(SIGNTOOL_KEY_ARGS "" CACHE STRING "Key arguments for signtool.exe - separate with ';'")
find_program(
  SIGNTOOL_EXE
  signtool
  PATHS
  "${WINDOWS_10_KIT_DIR}/x64"
  "${WINDOWS_10_KIT_DIR}/x86"
  DOC "Path to signtool.exe if SIGNTOOL_KEY_ARGS is set"
)

function(sign_target_file TARGET FILE)
  if(SIGNTOOL_KEY_ARGS AND WIN32)
    add_custom_command(
      TARGET ${TARGET} POST_BUILD
      COMMAND
      "${SIGNTOOL_EXE}"
      ARGS
      sign
      ${SIGNTOOL_KEY_ARGS}
      /t http://timestamp.digicert.com
      /fd SHA256
      "${FILE}"
    )
  endif()
endfunction()

function(sign_target TARGET)
  sign_target_file("${TARGET}" "$<TARGET_FILE:${TARGET}>")
endfunction()

macro(add_signed_script TARGET SOURCE)
  get_filename_component(FILE_NAME "${SOURCE}" NAME)
  add_custom_target(
    ${TARGET}
    ALL
    COMMAND
    "${CMAKE_COMMAND}"
    -E
    copy_if_different
    "${SOURCE}"
    "${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME}"
    WORKING_DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}"
    SOURCES
    "${SOURCE}"
  )

  sign_target_file(
    ${TARGET}
    "${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME}"
  )

  install(
    FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME}"
    ${ARGN}
  )
endmacro()
