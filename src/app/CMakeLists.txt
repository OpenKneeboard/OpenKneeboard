add_subdirectory(app-common)
add_subdirectory(app-winui3)

set(
  RUNTIME_FILES
  "$<TARGET_FILE:OpenKneeboard-OpenXR-Helper>"
  "$<TARGET_FILE:OpenKneeboard-Set-Desired-Elevation-Helper>"
  "$<TARGET_FILE:ThirdParty::Lua>"
  "${CMAKE_SOURCE_DIR}/src/dcs-hook/OpenKneeboardDCSExt.lua"
  "${CMAKE_BINARY_DIR}/src/injectables/OpenKneeboard-OpenXR.json"
  "$<GENEX_EVAL:$<TARGET_PROPERTY:ThirdParty::OpenVR,IMPORTED_LOCATION>>"
  "${CMAKE_SOURCE_DIR}/docs/Quick Start.pdf"
)

install(FILES "${RUNTIME_FILES}" TYPE BIN)

if(BUILD_BITNESS EQUAL 64)
  foreach(TARGET IN LISTS DUAL_ARCH_COMPONENTS)
    list(
      APPEND RUNTIME_FILES
      "$<GENEX_EVAL:$<TARGET_PROPERTY:${TARGET}32,IMPORTED_LOCATION>>"
    )
  endforeach()
  list(
    APPEND RUNTIME_FILES
    "$<PATH:GET_PARENT_PATH,$<GENEX_EVAL:$<TARGET_PROPERTY:OpenKneeboard-OpenXR32,IMPORTED_LOCATION>>>/OpenKneeboard-OpenXR32.json"
  )
endif()

set(
  DLL_TARGETS
  OpenKneeboard-AutoDetect
  OpenKneeboard-TabletProxy
  OpenKneeboard-WindowCaptureHook
  OpenKneeboard-nonvr-d3d11
  OpenKneeboard-oculus-d3d11
  OpenKneeboard-oculus-d3d12
  OpenKneeboard-OpenXR
  OpenKneeboard-c-api
  OpenKneeboard-lua-api
)
install(TARGETS ${DLL_TARGETS} LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}")

set(DLL_PDB_FILES)
foreach(TARGET ${DLL_TARGETS})
  list(APPEND RUNTIME_FILES "$<TARGET_FILE:${TARGET}>")
  list(APPEND DLL_PDB_FILES "$<TARGET_PDB_FILE:${TARGET}>")
  install(FILES "$<TARGET_PDB_FILE:${TARGET}>" DESTINATION "." COMPONENT DebugSymbols)
endforeach()

add_custom_target(
  CopyRuntimeFiles
  ALL
  COMMAND
  "${CMAKE_COMMAND}" -E make_directory
  "$<TARGET_FILE_DIR:OpenKneeboard-App-WinUI3>"
  &&
  ${CMAKE_COMMAND} -E copy_if_different
  "${RUNTIME_FILES}"
  "${DLL_PDB_FILES}"
  "$<TARGET_FILE_DIR:OpenKneeboard-App-WinUI3>"
  COMMAND_EXPAND_LISTS
)

add_custom_target(
  CopyDataFiles
  ALL
  COMMAND
  "${CMAKE_COMMAND}" -E make_directory
  "$<TARGET_FILE_DIR:OpenKneeboard-App-WinUI3>/../share"
  &&
  ${CMAKE_COMMAND} -E copy_if_different
  "${DATA_SOURCES}"
  "$<TARGET_FILE_DIR:OpenKneeboard-App-WinUI3>/../share"
  COMMAND_EXPAND_LISTS
)

add_dependencies(CopyRuntimeFiles ThirdParty::OpenVR)

if(BUILD_BITNESS EQUAL 64)
  foreach(TARGET IN LISTS DUAL_ARCH_COMPONENTS)
    add_dependencies(CopyRuntimeFiles "${TARGET}32")
  endforeach()
endif()
