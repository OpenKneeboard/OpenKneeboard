include_guard(GLOBAL)

function(ok_add_runtime_files TARGET)
  if (NOT ENABLE_APP)
    return()
  endif ()
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
  foreach (SOURCE IN LISTS ARG_UNPARSED_ARGUMENTS)
    list(APPEND SOURCES "$<PATH:ABSOLUTE_PATH,${SOURCE},${CMAKE_CURRENT_SOURCE_DIR}>")
  endforeach ()

  set(STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.stamp")
  add_custom_command(
    OUTPUT "${STAMP_FILE}"
    DEPENDS "${SOURCES}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEST}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SOURCES}" "${DEST}/"
    COMMAND "${CMAKE_COMMAND}" -E touch "${STAMP_FILE}"
    VERBATIM
    COMMAND_EXPAND_LISTS
  )
  add_custom_target(${TARGET} DEPENDS "${STAMP_FILE}")
  add_dependencies(runtime-dependencies "${TARGET}")

  list(LENGTH SOURCES SOURCES_LENGTH)
  if (SOURCES_LENGTH EQUAL 1)
    set_target_properties(
      "${TARGET}"
      PROPERTIES
      OUTPUT_FILE "${DEST}/$<PATH:GET_FILENAME,${SOURCES}>"
    )
  endif ()
endfunction()