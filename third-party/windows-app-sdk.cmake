include(ExternalProject)

ExternalProject_Add(
  WindowsAppSDKNuget
  URL "https://www.nuget.org/api/v2/package/Microsoft.WindowsAppSDK/1.0.0"
  URL_HASH "SHA256=b5319bc52f99cf2f9bb75ae1a77dce08ec2e57738b9d5a3c3849335bcd11c8ee"

  DEPENDS NuGet::CppWinRT::Exe

  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
)

ExternalProject_Get_property(WindowsAppSDKNuget SOURCE_DIR)

install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-cppwinrt.txt"
)

set(NUGET_WINDOWS_APP_SDK_PATH "${SOURCE_DIR}" PARENT_SCOPE)
