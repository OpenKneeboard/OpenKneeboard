include_guard(GLOBAL)

include(add_version_rc)

function(ok_postprocess_target TARGET)
  set(options RUNTIME_DEPENDENCY RUNTIME_EXE 32BIT_ONLY DUALARCH)
  set(valueArgs OUTPUT_NAME OUTPUT_SUBDIR)
  set(multiValueArgs)
  cmake_parse_arguments(POSTPROCESS_ARG "${options}" "${valueArgs}" "${multiValueArgs}" ${ARGN})

  get_target_property(ALIAS "${TARGET}" ALIASED_TARGET)

  if (ALIAS)
    return()
  endif ()

  if (POSTPROCESS_ARG_32BIT_ONLY OR POSTPROCESS_ARG_DUALARCH)
    set(BUILD_32BIT_TARGET ON)
  endif ()
  if (NOT POSTPROCESS_ARG_32BIT_ONLY)
    set(BUILD_64BIT_TARGET ON)
  endif ()

  if (POSTPROCESS_ARG_OUTPUT_SUBDIR)
    set(OUTPUT_SUBDIR ${POSTPROCESS_ARG_OUTPUT_SUBDIR})
  else ()
    set(OUTPUT_SUBDIR "bin")
  endif ()

  if (POSTPROCESS_ARG_OUTPUT_NAME)
    set(OUTPUT_NAME ${POSTPROCESS_ARG_OUTPUT_NAME})
  else ()
    get_target_property(OUTPUT_NAME "${TARGET}" OUTPUT_NAME)
    if (NOT OUTPUT_NAME)
      set(OUTPUT_NAME "${TARGET}")
    endif ()
  endif ()
  set(32BIT_OUTPUT_NAME "${OUTPUT_NAME}32")
  if (POSTPROCESS_ARG_DUALARCH)
    set(64BIT_OUTPUT_NAME "${OUTPUT_NAME}64")
  else ()
    set(64BIT_OUTPUT_NAME "${OUTPUT_NAME}")
  endif ()
  if (BUILD_IS_64BIT)
    set_target_properties(
      "${TARGET}"
      PROPERTIES
      OUTPUT_NAME "${64BIT_OUTPUT_NAME}"
    )
  endif ()
  if (BUILD_IS_32BIT)
    set_target_properties(
      "${TARGET}"
      PROPERTIES
      OUTPUT_NAME "${32BIT_OUTPUT_NAME}"
    )
  endif ()

  get_target_property(TARGET_TYPE "${TARGET}" TYPE)

  if (BUILD_32BIT_TARGET)
    set(32BIT_TARGET "${TARGET}32")
    add_library("${32BIT_TARGET}" INTERFACE IMPORTED GLOBAL)
    if (TARGET_TYPE STREQUAL "EXECUTABLE")
      set(IMPORTED_FILENAME "${32BIT_OUTPUT_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
    else ()
      set(IMPORTED_FILENAME "${32BIT_OUTPUT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    endif ()
    set(IMPORTED_LOCATION "${BUILD_OUT_ROOT}/${OUTPUT_SUBDIR}/${IMPORTED_FILENAME}")
    set_property(SOURCE "${IMPORTED_LOCATION}" PROPERTY GENERATED ON)

    set_target_properties(
      "${32BIT_TARGET}"
      PROPERTIES
      IMPORTED_FILENAME "${IMPORTED_FILENAME}"
      IMPORTED_LOCATION "${IMPORTED_LOCATION}"
    )

    if (BUILD_IS_64BIT)
      add_dependencies("${32BIT_TARGET}" build32)
      if (POSTPROCESS_ARG_RUNTIME_DEPENDENCY)
        add_dependencies(runtime-dependencies "${32BIT_TARGET}")
      endif ()
    endif ()
    if (BUILD_IS_32BIT)
      add_dependencies("${32BIT_TARGET}" "${TARGET}")
      add_dependencies(build32 ${TARGET})
    endif ()
  endif ()

  if (BUILD_32BIT_TARGET AND BUILD_IS_64BIT AND POSTPROCESS_ARG_RUNTIME_DEPENDENCY)
    cmake_path(GET IMPORTED_LOCATION STEM STEM)
    set(PDB_PATH "${BUILD_OUT_PDBDIR}/${STEM}.pdb")
  endif ()

  if ((BUILD_IS_64BIT AND NOT BUILD_64BIT_TARGET) OR (BUILD_IS_32BIT AND NOT BUILD_32BIT_TARGET))
    set_target_properties(
      "${TARGET}"
      PROPERTIES
      EXCLUDE_FROM_ALL ON
    )
    return()
  endif ()

  if (POSTPROCESS_ARG_RUNTIME_DEPENDENCY)
    add_dependencies(runtime-dependencies ${TARGET})
  endif ()

  if (NOT TARGET_TYPE MATCHES "^(EXECUTABLE|(SHARED|MODULE)_LIBRARY)$")
    return()
  endif ()

  add_version_rc(${TARGET})
  set_target_properties(
    ${TARGET}
    PROPERTIES
    PDB_OUTPUT_DIRECTORY "${BUILD_OUT_PDBDIR}"
  )
  if (POSTPROCESS_ARG_RUNTIME_DEPENDENCY OR POSTPROCESS_ARG_RUNTIME_EXE)
    set_target_properties(
      ${TARGET}
      PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${BUILD_OUT_ROOT}/${OUTPUT_SUBDIR}"
      LIBRARY_OUTPUT_DIRECTORY "${BUILD_OUT_ROOT}/${OUTPUT_SUBDIR}"
      PDB_OUTPUT_DIRECTORY "${BUILD_OUT_PDBDIR}"
    )
  endif ()
endfunction()