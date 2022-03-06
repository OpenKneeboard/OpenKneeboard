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

string(
  JOIN ";" LIBRARIES
  "$<TARGET_FILE:OpenKneeboard::App::Common>"
  "$<TARGET_FILE:OpenKneeboard-D2DErrorRenderer>"
  "$<TARGET_FILE:OpenKneeboard-DXResources>"
  "$<TARGET_FILE:OpenKneeboard-GameEvent>"
  "$<TARGET_FILE:OpenKneeboard-GetSystemColor>"
  "$<TARGET_FILE:OpenKneeboard-RuntimeFiles>"
  "$<TARGET_FILE:OpenKneeboard-SHM>"
  "$<TARGET_FILE:OpenKneeboard-UTF8>"
  "$<TARGET_FILE:OpenKneeboard-dprint>"
  "$<TARGET_FILE:OpenKneeboard-games>"
  "$<TARGET_FILE:OpenKneeboard-scope_guard>"
  "$<TARGET_FILE:OpenKneeboard-tablets>"
  "$<TARGET_FILE:ThirdParty::fmt>"
  "$<TARGET_FILE:ThirdParty::QPDF>"
  "$<GENEX_EVAL:$<TARGET_PROPERTY:ThirdParty::OpenVR,IMPORTED_IMPLIB>>"
  "$<GENEX_EVAL:$<TARGET_PROPERTY:ThirdParty::DirectXTK,INTERFACE_LINK_LIBRARIES>>"
  "$<GENEX_EVAL:$<TARGET_PROPERTY:ThirdParty::LibJpeg,INTERFACE_LINK_LIBRARIES>>"
  "$<GENEX_EVAL:$<TARGET_PROPERTY:ThirdParty::LibZip,INTERFACE_LINK_LIBRARIES>>"
  "$<GENEX_EVAL:$<TARGET_PROPERTY:ThirdParty::ZLib,INTERFACE_LINK_LIBRARIES>>"
)

add_custom_target(
  OpenKneeboard-App-WinUI3
  ALL
  COMMAND
  "${CMAKE_VS_MSBUILD_COMMAND}"
  "${CMAKE_CURRENT_SOURCE_DIR}\\app-winui3.vcxproj"
  "-p:Configuration=$<IF:$<CONFIG:Debug>,Debug,Release>"
  "-p:Platform=x64"

  "-p:CMakeIncludeDirs=\"$<SHELL_PATH:${INCLUDE_DIRS}>\""
  "-p:CMakeLinkLibs=\"$<SHELL_PATH:${LIBRARIES}>\""

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
)

set(OPENKNEEBOARD_WINUI3_APP_BUILD_DIR "${MSBUILD_OUTPUT_PATH}" PARENT_SCOPE)
