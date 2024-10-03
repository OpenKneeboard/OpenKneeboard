include_guard(GLOBAL)

include(ok_postprocess_target)

function(ok_add_executable TARGET)
  set(options RUNTIME_DEPENDENCY RUNTIME_EXE 32BIT_ONLY DUALARCH)
  set(valueArgs OUTPUT_SUBDIR)
  set(multiValueArgs)
  cmake_parse_arguments(EXECUTABLE_ARG "${options}" "${valueArgs}" "${multiValueArgs}" ${ARGN})

  add_executable("${TARGET}" ${EXECUTABLE_ARG_UNPARSED_ARGUMENTS})
  ok_postprocess_target("${TARGET}" ${ARGN})
endfunction()