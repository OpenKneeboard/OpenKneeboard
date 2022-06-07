include(ExternalProject)

ExternalProject_Add(
  WindowsAppSDKNuget
  URL "https://www.nuget.org/api/v2/package/Microsoft.WindowsAppSDK/1.1.0"
  URL_HASH "SHA256=03ea666f350eb97841bebe2a137cba915928079832662dc0b5d821e8953ed046"

  DEPENDS NuGet::CppWinRT::Exe

  PATCH_COMMAND
    cd <SOURCE_DIR>/
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
)

ExternalProject_Get_property(WindowsAppSDKNuget SOURCE_DIR)

install(
  FILES
  "${SOURCE_DIR}/license.txt"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-Windows-App-SDK.txt"
)

set(NUGET_WINDOWS_APP_SDK_PATH "${SOURCE_DIR}" PARENT_SCOPE)
