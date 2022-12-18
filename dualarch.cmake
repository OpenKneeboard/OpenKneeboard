set(
  DUAL_ARCH_COMPONENTS
  OpenKneeboard-WindowCaptureHook
  OpenKneeboard-WindowCaptureHook-Helper
  CACHE INTERNAL ""
)

# TODO (CMake 3.24+): use PATH:GET_FILENAME generator expression
# to infer this from IMPORTED_LOCATION
define_property(
  TARGET PROPERTY IMPORTED_FILENAME
  BRIEF_DOCS ""
  FULL_DOCS "")

if(BUILD_BITNESS EQUAL 32)
  add_custom_target(OpenKneeboard-dual-arch-components)
  add_dependencies(OpenKneeboard-dual-arch-components ${DUAL_ARCH_COMPONENTS})

  foreach(TARGET IN LISTS DUAL_ARCH_COMPONENTS)
    get_target_property(OUTPUT_NAME "${TARGET}" OUTPUT_NAME)

    if(NOT OUTPUT_NAME)
      set(OUTPUT_NAME "${TARGET}")
    endif()

    set_target_properties("${TARGET}" PROPERTIES OUTPUT_NAME "${OUTPUT_NAME}32")

    get_target_property(TARGET_TYPE "${TARGET}" TYPE)

    if(TARGET_TYPE STREQUAL "EXECUTABLE")
      install(TARGETS "${TARGET}" COMPONENT DualArch)
    else()
      install(TARGETS "${TARGET}" LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT DualArch)
    endif()

    install(FILES "$<TARGET_PDB_FILE:${TARGET}>" DESTINATION "." COMPONENT DualArchDebugSymbols)
  endforeach()
  return()
endif()

# 64-bit config here
foreach(TARGET IN LISTS DUAL_ARCH_COMPONENTS)
  get_target_property(OUTPUT_NAME "${TARGET}" OUTPUT_NAME)

  if(NOT OUTPUT_NAME)
    set(OUTPUT_NAME "${TARGET}")
  endif()

  set_target_properties("${TARGET}" PROPERTIES OUTPUT_NAME "${OUTPUT_NAME}64")
endforeach()

include(ExternalProject)

ExternalProject_Add(
  build-32bit-components
  SOURCE_DIR "${CMAKE_SOURCE_DIR}"
  STAMP_DIR "${CMAKE_BINARY_DIR}/stamp32"
  BINARY_DIR "${CMAKE_BINARY_DIR}/build32"
  INSTALL_DIR "${CMAKE_BINARY_DIR}/install32"
  DOWNLOAD_COMMAND ""
  CONFIGURE_COMMAND
  "${CMAKE_COMMAND}"
  "${CMAKE_SOURCE_DIR}"
  -A Win32
  "-DSIGNTOOL_KEY_ARGS=${SIGNTOOL_KEY_ARGS}"
  BUILD_COMMAND
  "${CMAKE_COMMAND}"
  --build .
  --config "$<CONFIG>"
  --target OpenKneeboard-dual-arch-components
  INSTALL_COMMAND
  "${CMAKE_COMMAND}"
  --install .
  --prefix "<INSTALL_DIR>/$<CONFIG>"
  --config "$<CONFIG>"
  --component DualArch
  &&
  "${CMAKE_COMMAND}"
  --install .
  --config "$<CONFIG>"
  --prefix "<INSTALL_DIR>/$<CONFIG>"
  --component DualArchDebugSymbols
)

ExternalProject_Get_property(build-32bit-components INSTALL_DIR)

foreach(TARGET IN LISTS DUAL_ARCH_COMPONENTS)
  add_library("${TARGET}32" INTERFACE IMPORTED GLOBAL)
  add_dependencies("${TARGET}32" build-32bit-components)

  get_target_property(TARGET_TYPE "${TARGET}" TYPE)

  if(TARGET_TYPE STREQUAL "EXECUTABLE")
    set_target_properties(
      "${TARGET}32"
      PROPERTIES
      IMPORTED_LOCATION "${INSTALL_DIR}/$<CONFIG>/bin/${TARGET}32.exe"
      IMPORTED_FILENAME "${TARGET}32.exe"
    )
  else()
    set_target_properties(
      "${TARGET}32"
      PROPERTIES
      IMPORTED_LOCATION "${INSTALL_DIR}/$<CONFIG>/bin/${TARGET}32.dll"
      IMPORTED_FILENAME "${TARGET}32.exe"
    )
  endif()

  install(
    FILES
    "${INSTALL_DIR}/$<CONFIG>/${TARGET}32.pdb"
    DESTINATION "."
    COMPONENT DebugSymbols
  )
endforeach()
