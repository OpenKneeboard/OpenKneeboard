set(
  CMAKE_CXX_STANDARD_LIBRARIES

  # Keep stuff that NuGet package references added
  "%(AdditionalDependencies)"
)

ok_add_executable(
  OpenKneeboard-App-WinUI3
  WIN32
  RUNTIME_EXE
  Resource.rc app.manifest
  HelpPage.idl HelpPage.xaml HelpPage.xaml.cpp HelpPage.xaml.h
  AdvancedSettingsPage.idl AdvancedSettingsPage.xaml AdvancedSettingsPage.xaml.cpp AdvancedSettingsPage.xaml.h
  App.idl App.xaml App.xaml.cpp App.xaml.h
  AppWindowViewModeDialog.idl AppWindowViewModeDialog.xaml AppWindowViewModeDialog.xaml.cpp AppWindowViewModeDialog.xaml.h
  BooleanToVisibilityConverter.cpp BooleanToVisibilityConverter.h BooleanToVisibilityConverter.idl
  CheckDCSHooks.cpp CheckDCSHooks.h
  CheckForUpdates.cpp CheckForUpdates.h
  DegreesValueConverter.cpp DegreesValueConverter.h DegreesValueConverter.idl
  ExecutableIconFactory.cpp ExecutableIconFactory.h
  FilePicker.cpp FilePicker.h
  Globals.cpp Globals.h
  HorizontalMirrorVRViewUIKind.cpp HorizontalMirrorVRViewUIKind.h
  IndependentVRViewUIKind.cpp IndependentVRViewUIKind.h
  IndependentVRViewSettingsControl.idl IndependentVRViewSettingsControl.xaml IndependentVRViewSettingsControl.xaml.cpp IndependentVRViewSettingsControl.xaml.h
  InputBindingsControl.idl InputBindingsControl.xaml InputBindingsControl.xaml.cpp InputBindingsControl.xaml.h
  InputDeviceUIData.cpp InputDeviceUIData.h
  InputSettingsPage.idl InputSettingsPage.xaml InputSettingsPage.xaml.cpp InputSettingsPage.xaml.h
  InstallPlugin.cpp InstallPlugin.h
  MainWindow.idl MainWindow.xaml MainWindow.xaml.cpp MainWindow.xaml.h
  MetersNumberFormatter.cpp MetersNumberFormatter.h MetersNumberFormatter.idl
  MultiplierValueConverter.cpp MultiplierValueConverter.h MultiplierValueConverter.idl
  NonVRSettingsPage.idl NonVRSettingsPage.xaml NonVRSettingsPage.xaml.cpp NonVRSettingsPage.xaml.h
  PercentValueConverter.cpp PercentValueConverter.h PercentValueConverter.idl
  ProfilesPage.idl ProfilesPage.xaml ProfilesPage.xaml.cpp ProfilesPage.xaml.h
  RenameTabDialog.idl RenameTabDialog.xaml RenameTabDialog.xaml.cpp RenameTabDialog.xaml.h
  SettingsPage.idl SettingsPage.xaml SettingsPage.xaml.cpp SettingsPage.xaml.h
  TabPage.idl TabPage.xaml TabPage.xaml.cpp TabPage.xaml.h
  TabsSettingsPage.idl TabsSettingsPage.xaml TabsSettingsPage.xaml.cpp TabsSettingsPage.xaml.h
  UIDataItem.idl UIDataItem.cpp UIDataItem.h
  VRSettingsPage.idl VRSettingsPage.xaml VRSettingsPage.xaml.cpp VRSettingsPage.xaml.h
  VRViewUIKinds.idl
  VRViewSettingsControl.idl VRViewSettingsControl.xaml VRViewSettingsControl.xaml.cpp VRViewSettingsControl.xaml.h
  WindowPickerDialog.idl WindowPickerDialog.xaml WindowPickerDialog.xaml.h WindowPickerDialog.xaml.cpp
  WithPropertyChangedEvent.cpp WithPropertyChangedEvent.h
)

include(winui3-boilerplate.cmake)
target_include_directories(
  OpenKneeboard-App-WinUI3
  PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/Generated Files"
)

set_property(
  SOURCE App.xaml
  PROPERTY VS_XAML_TYPE
  "ApplicationDefinition"
)

set_property(
  SOURCE InputDeviceUIData.h InputDeviceUIData.cpp
  PROPERTY VS_SETTINGS
  "DependentUpon=InputDeviceSettingsPage.idl"
)
target_link_libraries(
  OpenKneeboard-App-WinUI3
  PRIVATE
  OpenKneeboard-App-Common
  OpenKneeboard-ChromiumApp
  OpenKneeboard-ChromiumWorker
  OpenKneeboard-DXResources
  OpenKneeboard-FilesDiffer
  OpenKneeboard-GetMainHWND
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-Elevation
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-fatal
  OpenKneeboard-games
  OpenKneeboard-handles
  OpenKneeboard-semver
  OpenKneeboard-tracing
  OpenKneeboard-version
  System::Comctl32
  System::Dxgi
  System::Shell32
  System::User32
  ThirdParty::LibZip
)
target_link_options(
  OpenKneeboard-App-WinUI3
  PRIVATE
  # Needs library path mutating first
  "/DELAYLOAD:libcef.dll"
  # Interfere with CEF sandbox
  "/DELAYLOAD:Windows.Data.Pdf.dll"
)

target_include_directories(
  OpenKneeboard-App-WinUI3
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

set(BUILD_TIME_MARKER_FILE "${CMAKE_CURRENT_BINARY_DIR}/openkneeboard-build-time-marker.hpp")
add_custom_command(
  OUTPUT "${BUILD_TIME_MARKER_FILE}"
  COMMAND "${CMAKE_COMMAND}" -E touch "${BUILD_TIME_MARKER_FILE}"
)
target_sources(OpenKneeboard-App-WinUI3 PRIVATE "${BUILD_TIME_MARKER_FILE}")

target_precompile_headers(
  OpenKneeboard-App-WinUI3
  PRIVATE
  pch.h
)

target_compile_definitions(
  OpenKneeboard-App-WinUI3
  PRIVATE
  DISABLE_XAML_GENERATED_MAIN
  MICROSOFT_WINDOWSAPPSDK_UNDOCKEDREGFREEWINRT_AUTO_INITIALIZE_LOADLIBRARY
)

target_compile_options(
  OpenKneeboard-App-WinUI3
  PRIVATE

  # We intentionally throw from noexcept functions to get more useful
  # backtraces/crash dumps from winrt event handlers
  "/wd4297"
  "/FI${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/OpenKneeboard-App-WinUI3.dir/$<CONFIG>/cmake_pch.hxx"
)

set_target_properties(
  OpenKneeboard-App-WinUI3
  PROPERTIES
  OUTPUT_NAME "OpenKneeboardApp"
)