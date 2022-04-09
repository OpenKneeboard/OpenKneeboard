# So, WinUI3 looks good and has powerful layouts. Sadly:
# - While controls can be created through code, XAML is required
#   for things like Frame::Navigate
# - The XAML compiler is only available as an msbuild DLL
#
# ... so, for now, we need to use msbuild for this. Hopefully it
# changes in the future.
set(MSBUILD_GENERATED_FILES_PATH "${CMAKE_CURRENT_BINARY_DIR}/msbuild-generated-files/")
set(MSBUILD_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/msbuild-out-$<CONFIG>/")
set(MSBUILD_INTERMEDIATE_PATH "${CMAKE_CURRENT_BINARY_DIR}/msbuild-intermediate-$<CONFIG>/")

string(
  JOIN ";" INCLUDE_DIRS
  "$<TARGET_PROPERTY:OpenKneeboard-App-Common,INCLUDE_DIRECTORIES>"
)

set(
  TARGET_FILE_LIBRARIES
  OpenKneeboard::App::Common
  OpenKneeboard-D2DErrorRenderer
  OpenKneeboard-DXResources
  OpenKneeboard-GameEvent
  OpenKneeboard-GetSystemColor
  OpenKneeboard-RayIntersectsRect
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-SHM
  OpenKneeboard-UTF8
  OpenKneeboard-dprint
  OpenKneeboard-games
  OpenKneeboard-scope_guard
  OpenKneeboard-tablets
  ThirdParty::fmt
  ThirdParty::QPDF
)

set(IMPORTED_IMPLIB_LIBRARIES ThirdParty::OpenVR)

set(
  INTERFACE_LIBRARIES
  ThirdParty::DirectXTK
  ThirdParty::LibJpeg
  ThirdParty::LibZip
  ThirdParty::ZLib
)

set(LIBRARY_DEPENDENCIES)
set(LIBRARY_PATHS)

foreach(TARGET ${TARGET_FILE_LIBRARIES})
  list(APPEND LIBRARY_DEPENDENCIES "${TARGET}")
  list(APPEND LIBRARY_PATHS "$<TARGET_FILE:${TARGET}>")
endforeach()

foreach(TARGET ${IMPORTED_IMPLIB_LIBRARIES})
  list(APPEND LIBRARY_DEPENDENCIES "${TARGET}")
  list(
    APPEND
    LIBRARY_PATHS
    "$<GENEX_EVAL:$<TARGET_PROPERTY:${TARGET},IMPORTED_IMPLIB>>"
  )
endforeach()

foreach(TARGET ${INTERFACE_LIBRARIES})
  list(APPEND LIBRARY_DEPENDENCIES "${TARGET}")
  list(
    APPEND
    LIBRARY_PATHS
    "$<GENEX_EVAL:$<TARGET_PROPERTY:${TARGET},INTERFACE_LINK_LIBRARIES>>"
  )
endforeach()

string(JOIN ";" LIBRARY_PATHS ${LIBRARY_PATHS})

set(VERSION_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
create_version_rc(
  OpenKneeboard-App-WinUI3_version-rc
  OpenKneeboard-App-WinUI3
  OpenKneeboardApp.exe
  ${VERSION_RC_FILE}
)
get_filename_component(VERSION_RC_DIRECTORY "${VERSION_RC_FILE}" DIRECTORY)

add_custom_target(
  OpenKneeboard-App-WinUI3
  ALL
  COMMAND
  "${CMAKE_VS_MSBUILD_COMMAND}"
  "${CMAKE_CURRENT_SOURCE_DIR}\\app-winui3.vcxproj"
  "-p:Configuration=$<IF:$<CONFIG:Debug>,Debug,Release>"
  "-p:Platform=x64"

  "-p:CMakeIncludeDirs=\"$<SHELL_PATH:${INCLUDE_DIRS}>\""
  "-p:CMakeLinkLibs=\"$<SHELL_PATH:${LIBRARY_PATHS}>\""
  "-p:VERSION_RC_DIRECTORY=\"$<SHELL_PATH:${VERSION_RC_DIRECTORY}>\""

  "-p:NUGET_CPPWINRT_PATH=${NUGET_CPPWINRT_PATH}" 
  "-p:NUGET_WINDOWS_APP_SDK_PATH=${NUGET_WINDOWS_APP_SDK_PATH}" 
  "-p:NUGET_WINDOWS_SDK_BUILD_TOOLS_PATH=${NUGET_WINDOWS_SDK_BUILD_TOOLS_PATH}" 
  "-p:NUGET_WINDOWS_IMPLEMENTATION_LIBRARY_PATH=${NUGET_WINDOWS_IMPLEMENTATION_LIBRARY_PATH}" 

  "-p:BaseOutputPath=$<SHELL_PATH:${MSBUILD_OUTPUT_PATH}>"
  "-p:OutputPath=$<SHELL_PATH:${MSBUILD_OUTPUT_PATH}>"
  "-p:OutputDir=$<SHELL_PATH:${MSBUILD_OUTPUT_PATH}>"
  "-p:OutDir=$<SHELL_PATH:${MSBUILD_OUTPUT_PATH}>"
  "-p:IntDir=$<SHELL_PATH:${MSBUILD_INTERMEDIATE_PATH}>"
  "-p:GeneratedFilesDir=$<SHELL_PATH:${MSBUILD_GENERATED_FILES_PATH}>"
  DEPENDS
  OpenKneeboard-App-Common
  CppWinRTNuget
  WindowsImplementationLibraryNuget
  WindowsAppSDKNuget
  WindowsSDKBuildToolsNuget
  ${VERSION_RC_FILE}
  ${LIBRARY_DEPENDENCIES}
)

set(OPENKNEEBOARD_WINUI3_APP_BUILD_DIR "${MSBUILD_OUTPUT_PATH}" PARENT_SCOPE)

file(GLOB XAML_FILES CONFIGURE_DEPENDS "*.xaml")
set(XBF_FILES)
foreach(XAML_FILE ${XAML_FILES})
  get_filename_component(BASENAME "${XAML_FILE}" NAME_WLE)
  list(APPEND XBF_FILES "${MSBUILD_OUTPUT_PATH}/${BASENAME}.xbf")
endforeach()

install(
  FILES
  "${MSBUILD_OUTPUT_PATH}/OpenKneeboardApp.exe"
  "${MSBUILD_OUTPUT_PATH}/Microsoft.WindowsAppRuntime.Bootstrap.dll"
  ${XBF_FILES}
  TYPE BIN
)
install(
  FILES
  "${MSBUILD_OUTPUT_PATH}/OpenKneeboardApp.pdb"
  TYPE BIN
  COMPONENT DebugSymbols
)
