macro(ok_add_license_file SOURCE DESTINATION)
  set(options)
  set(valueArgs DEPENDS)
  set(multiValueArgs)
  cmake_parse_arguments(arg "${options}" "${valueArgs}" "${multiValueArgs}" ${ARGN})
  set(DESTINATION_PATH "${BUILD_OUT_ROOT}/share/doc/${DESTINATION}")

  if (NOT arg_DEPENDS)
    file(
      GENERATE
      OUTPUT "${DESTINATION_PATH}"
      INPUT "${SOURCE}"
    )
  else ()
    cmake_path(GET DESTINATION_PATH PARENT_PATH DESTINATION_DIRECTORY)
    add_custom_command(
      TARGET "${arg_DEPENDS}"
      POST_BUILD
      COMMAND
      "${CMAKE_COMMAND}" -E make_directory "${DESTINATION_DIRECTORY}"
      COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different
      "${SOURCE}"
      "${DESTINATION_PATH}"
      VERBATIM
    )
  endif ()
endmacro()