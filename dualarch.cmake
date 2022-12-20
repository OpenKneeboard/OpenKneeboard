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
  message(STATUS "Preparing 32-bit build of 32bit-runtime-components")
  add_custom_target(OpenKneeboard-32bit-runtime-components)
  add_dependencies(OpenKneeboard-32bit-runtime-components ${DUAL_ARCH_COMPONENTS})

  foreach(TARGET IN LISTS DUAL_ARCH_COMPONENTS)
    add_custom_target("${TARGET}32" INTERFACE IMPORTED GLOBAL)
    set_target_properties(
      "${TARGET}32"
      PROPERTIES
      IMPORTED_LOCATION "IGNORE"
      IMPORTED_FILENAME "IGNORE"
    )

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

message(STATUS "Preparing 64-bit -> 32-bit subbuild of dual-arch components")

ExternalProject_Add(
  thirtyTwoBitBuild
  SOURCE_DIR "${CMAKE_SOURCE_DIR}"
  STAMP_DIR "${CMAKE_BINARY_DIR}/stamp32"
  BINARY_DIR "${CMAKE_BINARY_DIR}/build32"
  INSTALL_DIR "${CMAKE_BINARY_DIR}/install32"
  DOWNLOAD_COMMAND ""
  CONFIGURE_COMMAND
  "${CMAKE_COMMAND}"
  -S "<SOURCE_DIR>"
  -B "<BINARY_DIR>"
  -A Win32
  BUILD_COMMAND
  echo "${CMAKE_COMMAND}"
  &&
  "${CMAKE_COMMAND}"
  --build "<BINARY_DIR>"
  --config "$<CONFIG>"
  --target OpenKneeboard-32bit-runtime-components
  INSTALL_COMMAND
  "${CMAKE_COMMAND}"
  --install "<BINARY_DIR>"
  --prefix "<INSTALL_DIR>/$<CONFIG>"
  --config "$<CONFIG>"
  --component DualArch
  &&
  "${CMAKE_COMMAND}"
  --install "<BINARY_DIR>"
  --config "$<CONFIG>"
  --prefix "<INSTALL_DIR>/$<CONFIG>"
  --component DualArchDebugSymbols
)

ExternalProject_Get_property(thirtyTwoBitBuild INSTALL_DIR)

add_library(OpenKneeboard-32bit-runtime-components INTERFACE)

foreach(TARGET IN LISTS DUAL_ARCH_COMPONENTS)
  add_library("${TARGET}32" INTERFACE IMPORTED GLOBAL)
  add_dependencies("${TARGET}32" thirtyTwoBitBuild)
  add_dependencies(OpenKneeboard-32bit-runtime-components "${TARGET}32")

  get_target_property(TARGET_TYPE "${TARGET}" TYPE)

  if(TARGET_TYPE STREQUAL "EXECUTABLE")
    set(IMPORTED_FILENAME "${TARGET}32.exe")
  else()
    set(IMPORTED_FILENAME "${TARGET}32.dll")
  endif()

  set(IMPORTED_LOCATION "${INSTALL_DIR}/$<CONFIG>/bin/${IMPORTED_FILENAME}")
  set_target_properties(
    "${TARGET}32"
    PROPERTIES
    IMPORTED_LOCATION "${IMPORTED_LOCATION}"
    IMPORTED_FILENAME "${IMPORTED_FILENAME}"
  )
  sign_target_file(
    thirtyTwoBitBuild
    "${IMPORTED_LOCATION}"
  )

  install(
    FILES
    "${IMPORTED_LOCATION}"
    TYPE BIN
    COMPONENT Default
  )
  install(
    FILES
    "${INSTALL_DIR}/$<CONFIG>/${TARGET}32.pdb"
    DESTINATION "."
    COMPONENT DebugSymbols
  )
endforeach()
