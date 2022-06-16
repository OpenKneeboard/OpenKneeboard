# Patch up the generated vcxproj
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.targets"
  "${CMAKE_CURRENT_BINARY_DIR}/Directory.Build.targets"
  COPYONLY
)

set_property(
  TARGET OpenKneeboard-App-WinUI3
  PROPERTY VS_PACKAGE_REFERENCES
  "Microsoft.Windows.CppWinRT_2.0.220608.4"
  "Microsoft.WindowsAppSDK_1.1.1"
  "Microsoft.Windows.SDK.BuildTools_10.0.22621.1"
  "Microsoft.Windows.ImplementationLibrary_1.0.220201.1"
)

set_target_properties(
  OpenKneeboard-App-WinUI3
  PROPERTIES
  # ----- C++/WinRT, Windows App SDK, and WinUI stuff starts here -----
  VS_GLOBAL_RootNamespace OpenKneeboardApp
  VS_GLOBAL_AppContainerApplication false
  VS_GLOBAL_AppxPackage false
  VS_GLOBAL_CppWinRTOptimized true
  VS_GLOBAL_CppWinRTRootNamespaceAutoMerge true
  VS_GLOBAL_UseWinUI true
  VS_GLOBAL_ApplicationType "Windows Store"
  VS_GLOBAL_WindowsPackageType None
  VS_GLOBAL_EnablePreviewMsixTooling true
  VS_GLOBAL_WindowsAppSDKSelfContained true
)

# Set source file dependencies properly for Xaml and non-Xaml IDL
# files.
#
# Without this, `module.g.cpp` will not include the necessary headers
# for non-Xaml IDL files, e.g. value converters
get_target_property(SOURCES OpenKneeboard-App-WinUI3 SOURCES)
foreach(SOURCE ${SOURCES})
  cmake_path(GET SOURCE EXTENSION LAST_ONLY EXTENSION)
  if(NOT "${EXTENSION}" STREQUAL ".idl")
    continue()
  endif()
  set(IDL_SOURCE "${SOURCE}")

  cmake_path(REMOVE_EXTENSION SOURCE LAST_ONLY OUTPUT_VARIABLE BASENAME)
  set(XAML_SOURCE "${BASENAME}.xaml")
  if("${XAML_SOURCE}" IN_LIST SOURCES)
    set_property(
      SOURCE "${IDL_SOURCE}"
      PROPERTY VS_SETTINGS
      "SubType=Code"
      "DependentUpon=${XAML_SOURCE}"
    )
  else()
    set_property(
      SOURCE "${IDL_SOURCE}"
      PROPERTY VS_SETTINGS
      "SubType=Code"
    )
    set_property(
      SOURCE "${BASENAME}.h"
      PROPERTY VS_SETTINGS
      "DependentUpon=${IDL_SOURCE}"
    )
    set_property(
      SOURCE "${BASENAME}.cpp"
      PROPERTY VS_SETTINGS
      "DependentUpon=${IDL_SOURCE}"
    )
  endif()
endforeach()
