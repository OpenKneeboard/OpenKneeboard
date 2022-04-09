add_subdirectory(app-common)
add_subdirectory(app-winui3)

set(
  RUNTIME_FILES
  "$<TARGET_FILE:OpenKneeboard-OpenXR-RegisterLayer>"
  "${CMAKE_SOURCE_DIR}/src/dcs-hook/OpenKneeboardDCSExt.lua"
  "${CMAKE_SOURCE_DIR}/src/injectables/OpenKneeboard-OpenXR.json"
  "$<GENEX_EVAL:$<TARGET_PROPERTY:ThirdParty::OpenVR,IMPORTED_LOCATION>>"
)
install(FILES "${RUNTIME_FILES}" TYPE BIN)

set(
  DLL_TARGETS
  OpenKneeboard-Injection-Bootstrapper
  OpenKneeboard-AutoInject-Marker
  OpenKneeboard-TabletProxy
  OpenKneeboard-nonvr-d3d11
  OpenKneeboard-oculus-d3d11
  OpenKneeboard-oculus-d3d12
  OpenKneeboard-OpenXR
  OpenKneeboard-dcs-ext-dll
)
install(TARGETS ${DLL_TARGETS} LIBRARY DESTINATION bin)

foreach(TARGET ${DLL_TARGETS})
  list(APPEND RUNTIME_FILES "$<TARGET_FILE:${TARGET}>")
  install(FILES "$<TARGET_PDB_FILE:${TARGET}>" TYPE BIN COMPONENT DebugSymbols)
endforeach()

add_custom_target(
	CopyRuntimeFiles
	ALL
	COMMAND
	"${CMAKE_COMMAND}" -E make_directory
	"${OPENKNEEBOARD_WINUI3_APP_BUILD_DIR}"
	&&
	${CMAKE_COMMAND} -E copy_if_different
	"${RUNTIME_FILES}"
	"${OPENKNEEBOARD_WINUI3_APP_BUILD_DIR}"
	COMMAND_EXPAND_LISTS
)
add_dependencies(CopyRuntimeFiles ThirdParty::OpenVR)
add_dependencies(OpenKneeboard-App-WinUI3 CopyRuntimeFiles)


set(GENERATED_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/appxmanifest.xml")
add_custom_target(
  AppxManifest
  ALL
  COMMAND
  ${CMAKE_COMMAND}
  -DVERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
  -DVERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
  -DVERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
  -DVCLIBS_EXTRA="$<$<CONFIG:Debug>:.Debug>"
  "-DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/appxmanifest.xml.in"
  "-DOUTPUT_FILE=${GENERATED_MANIFEST}"
  "-P${CMAKE_CURRENT_SOURCE_DIR}/appxmanifest.xml.cmake"
  BYPRODUCTS
  "${GENERATED_MANIFEST}"
  WORKING_DIRECTORY
  "${CMAKE_SOURCE_DIR}"
)
install(FILES "${GENERATED_MANIFEST}" DESTINATION .)
