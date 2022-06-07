include(ExternalProject)

ExternalProject_Add(
  CppWinRTNuget
  URL "https://github.com/microsoft/cppwinrt/releases/download/2.0.220418.1/Microsoft.Windows.CppWinRT.2.0.220418.1.nupkg"
  URL_HASH "SHA256=2b80864729ac995915a391cda1e827e03990a511f5217863da3626a558934e89"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND "<SOURCE_DIR>/bin/cppwinrt.exe" -in local -output "<BINARY_DIR>/include"
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
)

ExternalProject_Get_property(CppWinRTNuget SOURCE_DIR)
ExternalProject_Get_property(CppWinRTNuget BINARY_DIR)

add_executable(cppwinrt-exe IMPORTED GLOBAL)
add_dependencies(cppwinrt-exe CppWinRTNuget)
set_target_properties(cppwinrt-exe PROPERTIES IMPORTED_LOCATION "${SOURCE_DIR}/bin/cppwinrtt.exe")
add_executable(NuGet::CppWinRT::Exe ALIAS cppwinrt-exe)

add_library(cppwinrt-base INTERFACE)
add_dependencies(cppwinrt-base CppWinRTNuget)
target_link_libraries(cppwinrt-base INTERFACE WindowsApp.lib)
target_include_directories(cppwinrt-base INTERFACE "${BINARY_DIR}/include")
add_library(NuGet::CppWinRT::Base ALIAS cppwinrt-base)

install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-cppwinrt.txt"
)

set(NUGET_CPPWINRT_PATH "${SOURCE_DIR}" PARENT_SCOPE)
