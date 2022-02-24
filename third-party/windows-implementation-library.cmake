include(ExternalProject)

ExternalProject_Add(
  WindowsImplementationLibraryNuget
  URL "https://www.nuget.org/api/v2/package/Microsoft.Windows.ImplementationLibrary/1.0.220201.1"
  URL_HASH "SHA256=924f1553bf191578261312bedfa9e9a441ad9fca59aaeae4f0038aa3533ccd34"

  DEPENDS NuGet::CppWinRT::Exe

  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
)

ExternalProject_Get_property(WindowsImplementationLibraryNuget SOURCE_DIR)

install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-cppwinrt.txt"
)

set(NUGET_WINDOWS_IMPLEMENTATION_LIBRARY_PATH "${SOURCE_DIR}" PARENT_SCOPE)
