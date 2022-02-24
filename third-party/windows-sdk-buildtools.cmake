include(ExternalProject)

ExternalProject_Add(
	WindowsSDKBuildToolsNuget
	URL "https://www.nuget.org/api/v2/package/Microsoft.Windows.SDK.BuildTools/10.0.22000.197"
	URL_HASH "SHA256=8619a033bc5a7a86fad708fcb3496435662305e2061cb0a41352a87b608ee366"

	DEPENDS NuGet::CppWinRT::Exe

	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
)

ExternalProject_Get_property(WindowsSDKBuildToolsNuget SOURCE_DIR)

set(NUGET_WINDOWS_SDK_BUILD_TOOLS_PATH "${SOURCE_DIR}" PARENT_SCOPE)
