set(
  CMAKE_CXX_STANDARD_LIBRARIES

  # Keep stuff that NuGet package references added
  "%(AdditionalDependencies)"
)

ok_add_executable(
  OpenKneeboard-App-WinUI3
  WIN32
  Resource.rc app.manifest
  HelpPage.idl HelpPage.xaml HelpPage.xaml.cpp HelpPage.xaml.h
  AdvancedSettingsPage.idl AdvancedSettingsPage.xaml AdvancedSettingsPage.xaml.cpp AdvancedSettingsPage.xaml.h
  App.idl App.xaml App.xaml.cpp App.xaml.h
  BooleanToVisibilityConverter.cpp BooleanToVisibilityConverter.h BooleanToVisibilityConverter.idl
  CheckDCSHooks.cpp CheckDCSHooks.h
  CheckForUpdates.cpp CheckForUpdates.h
  DegreesValueConverter.cpp DegreesValueConverter.h DegreesValueConverter.idl
  ExecutableIconFactory.cpp ExecutableIconFactory.h
  FilePicker.cpp FilePicker.h
  GameSettingsPage.idl GameSettingsPage.xaml GameSettingsPage.xaml.cpp GameSettingsPage.xaml.h
  Globals.cpp Globals.h
  HorizontalMirrorVRViewUIKind.cpp HorizontalMirrorVRViewUIKind.h
  IndependentVRViewUIKind.cpp IndependentVRViewUIKind.h
  IndependentVRViewSettingsControl.idl IndependentVRViewSettingsControl.xaml IndependentVRViewSettingsControl.xaml.cpp IndependentVRViewSettingsControl.xaml.h
  InputBindingsControl.idl InputBindingsControl.xaml InputBindingsControl.xaml.cpp InputBindingsControl.xaml.h
  InputDeviceUIData.cpp InputDeviceUIData.h
  InputSettingsPage.idl InputSettingsPage.xaml InputSettingsPage.xaml.cpp InputSettingsPage.xaml.h
  MainWindow.idl MainWindow.xaml MainWindow.xaml.cpp MainWindow.xaml.h
  MetersNumberFormatter.cpp MetersNumberFormatter.h MetersNumberFormatter.idl
  MultiplierValueConverter.cpp MultiplierValueConverter.h MultiplierValueConverter.idl
  NonVRSettingsPage.idl NonVRSettingsPage.xaml NonVRSettingsPage.xaml.cpp NonVRSettingsPage.xaml.h
  PercentValueConverter.cpp PercentValueConverter.h PercentValueConverter.idl
  ProcessPickerDialog.idl ProcessPickerDialog.xaml ProcessPickerDialog.xaml.cpp ProcessPickerDialog.xaml.h
  ProfilesPage.idl ProfilesPage.xaml ProfilesPage.xaml.cpp ProfilesPage.xaml.h
  RenameTabDialog.idl RenameTabDialog.xaml RenameTabDialog.xaml.cpp RenameTabDialog.xaml.h
  SettingsPage.idl SettingsPage.xaml SettingsPage.xaml.cpp SettingsPage.xaml.h
  SponsorButton.idl SponsorButton.xaml SponsorButton.xaml.cpp SponsorButton.xaml.h
  TabPage.idl TabPage.xaml TabPage.xaml.cpp TabPage.xaml.h
  TabSettingsPage.idl TabSettingsPage.xaml TabSettingsPage.xaml.cpp TabSettingsPage.xaml.h
  VRSettingsPage.idl VRSettingsPage.xaml VRSettingsPage.xaml.cpp VRSettingsPage.xaml.h
  VRViewUIKind.cpp VRViewUIKind.h
  VRViewSettingsControl.idl VRViewSettingsControl.xaml VRViewSettingsControl.xaml.cpp VRViewSettingsControl.xaml.h
  WindowPickerDialog.idl WindowPickerDialog.xaml WindowPickerDialog.xaml.h WindowPickerDialog.xaml.cpp
  WithPropertyChangedEvent.cpp WithPropertyChangedEvent.h
)

include(winui3-boilerplate.cmake)

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
  OpenKneeboard-DXResources
  OpenKneeboard-FilesDiffer
  OpenKneeboard-GetMainHWND
  OpenKneeboard-RuntimeFiles
  OpenKneeboard-Elevation
  OpenKneeboard-OpenXRMode
  OpenKneeboard-RunSubprocessAsync
  OpenKneeboard-config
  OpenKneeboard-dprint
  OpenKneeboard-32bit-runtime-components
  OpenKneeboard-games
  OpenKneeboard-handles
  OpenKneeboard-version
  System::Comctl32
  System::Dbghelp
  System::Dxgi
  System::Shell32
  System::User32
  ThirdParty::LibZip
  ThirdParty::SemVer
)

target_include_directories(
  OpenKneeboard-App-WinUI3
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_precompile_headers(
  OpenKneeboard-App-WinUI3
  PRIVATE
  pch.h
)

target_compile_definitions(
  OpenKneeboard-App-WinUI3
  PRIVATE
  DISABLE_XAML_GENERATED_MAIN=1
)

target_compile_options(
  OpenKneeboard-App-WinUI3
  PRIVATE

  # We intentionally throw from noexcept functions to get more useful
  # backtraces/crash dumps from winrt event handlers
  "/wd4297"
  "/FI${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/OpenKneeboard-App-WinUI3.dir/$<CONFIG>/cmake_pch.hxx"
)

set_property(
  TARGET OpenKneeboard-App-WinUI3
  PROPERTY OUTPUT_NAME OpenKneeboardApp # .exe
)

install(
  DIRECTORY

  # Trailing / copies the contents instead of the directory itself,
  # i.e. avoids installing `bin/Debug/`
  "$<TARGET_FILE_DIR:OpenKneeboard-App-WinUI3>/"
  TYPE BIN
  PATTERN "*.pdb" EXCLUDE
  PATTERN "*.lib" EXCLUDE
  # WinUI3 includes these localizations, but they are not supported by WiX v3.11.2, so including them breaks the installer :'(
  PATTERN "gd-gb/*" EXCLUDE
  PATTERN "mi-NZ/*" EXCLUDE
  PATTERN "ug-CN/*" EXCLUDE
  # These are just put here for use during development, but are put in `share\` for the real installation
  PATTERN "DCS-Aircraft-Mapping.json" EXCLUDE
)

set_property(
  INSTALL
  "OpenKneeboardApp.exe"
  PROPERTY
  CPACK_START_MENU_SHORTCUTS "OpenKneeboard"
)
set_property(
  INSTALL
  "OpenKneeboardApp.exe"
  PROPERTY
  CPACK_DESKTOP_SHORTCUTS "OpenKneeboard"
)
