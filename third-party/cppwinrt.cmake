include(ExternalProject)

ExternalProject_Add(
  cppwinrtBuild
  URL "https://github.com/microsoft/cppwinrt/releases/download/2.0.220131.2/Microsoft.Windows.CppWinRT.2.0.220131.2.nupkg"
  URL_HASH "SHA256=899c1c676c72404aea4c07ebd7e3314c245867f95918c79fc122642df85e168c"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND "<SOURCE_DIR>/bin/cppwinrt.exe" -in local -output "<BINARY_DIR>/include"
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
)

add_library(cppwinrt INTERFACE)
add_dependencies(cppwinrt cppwinrtBuild)

ExternalProject_Get_property(cppwinrtBuild BINARY_DIR)
target_include_directories(cppwinrt INTERFACE "${BINARY_DIR}/include")

install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-cppwinrt.txt"
)
