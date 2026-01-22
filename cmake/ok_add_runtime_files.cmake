include_guard(GLOBAL)

function(ok_add_runtime_files TARGET)
  set(options)
  set(valueArgs DESTINATION)
  set(multiValueArgs)
  cmake_parse_arguments(ARG "${options}" "${valueArgs}" "${multiValueArgs}" ${ARGN})


  set(DEST_SUBDIR bin)
  if (ARG_DESTINATION)
    set(DEST_SUBDIR "${ARG_DESTINATION}")
  endif ()
  set(DEST "${BUILD_OUT_ROOT}/${DEST_SUBDIR}")

  set(SOURCES)
  set(OUTPUTS)
  foreach (SOURCE IN LISTS ARG_UNPARSED_ARGUMENTS)
    cmake_path(ABSOLUTE_PATH SOURCE BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    list(APPEND SOURCES "${SOURCE}")

    cmake_path(GET SOURCE FILENAME FILENAME)
    list(APPEND OUTPUTS "${DEST}/${FILENAME}")
  endforeach ()

  add_custom_command(
    OUTPUT "${OUTPUTS}"
    DEPENDS "${SOURCES}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SOURCES}" "${DEST}/"
    VERBATIM
    COMMAND_EXPAND_LISTS
  )
  add_custom_target(${TARGET} DEPENDS "${OUTPUTS}")
  add_dependencies(runtime-dependencies "${TARGET}")
endfunction()