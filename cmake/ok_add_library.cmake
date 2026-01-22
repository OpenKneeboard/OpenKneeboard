include_guard(GLOBAL)

include(ok_postprocess_target)

function(ok_add_library TARGET KIND)
  set(options DUALARCH RUNTIME_DEPENDENCY)
  set(valueArgs OUTPUT_NAME)
  set(multiValueArgs HEADERS INCLUDE_DIRECTORIES)
  cmake_parse_arguments(LIBRARY_ARG "${options}" "${valueArgs}" "${multiValueArgs}" ${ARGN})

  add_library("${TARGET}" "${KIND}" ${LIBRARY_ARG_UNPARSED_ARGUMENTS})
  if (LIBRARY_ARG_HEADERS)
    if ("$KIND" STREQUAL "INTERFACE")
      set(HEADER_KIND "INTERFACE")
    else ()
      set(HEADER_KIND "PUBLIC")
    endif ()
    target_sources(
      "${TARGET}"
      "${HEADER_KIND}"
      FILE_SET HEADERS
      TYPE HEADERS
      BASE_DIRS ${LIBRARY_ARG_INCLUDE_DIRECTORIES}
      FILES ${LIBRARY_ARG_HEADERS}
    )
  endif ()

  if ("${KIND}" STREQUAL "MODULE")
    list(APPEND POSTPROCESS_ARGS RUNTIME_DEPENDENCY)
  endif ()
  ok_postprocess_target("${TARGET}" ${POSTPROCESS_ARGS} ${ARGN})

  if ("${KIND}" STREQUAL "INTERFACE" AND "$ENV{CLION_IDE}")
    # Workaround https://youtrack.jetbrains.com/issue/CPP-11340
    add_executable("${TARGET}-clion-helper" EXCLUDE_FROM_ALL "${CMAKE_SOURCE_DIR}/scripts/true.cpp")
    target_link_libraries("${TARGET}-clion-helper" PRIVATE "${TARGET}")
  endif ()
endfunction()