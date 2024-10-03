include_guard(GLOBAL)

include(FetchContent)

# Used for nuget
set(WINDOWS_IMPLEMENTATION_LIBRARY_VERSION "1.0.240122.1" CACHE INTERNAL "")

FetchContent_Declare(
  wil_nuget
  URL "https://www.nuget.org/api/v2/package/Microsoft.Windows.ImplementationLibrary/1.0.240122.1"
  URL_HASH "SHA256=2762643e7865c42ac60d809fe951a813ae10960141e56bb74ad28843f2d6b5c1"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(wil_nuget)

add_library(ThirdParty::WIL INTERFACE IMPORTED GLOBAL)
set_target_properties(
  ThirdParty::WIL
  PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${wil_nuget_SOURCE_DIR}/include"
  VERSION "${WINDOWS_IMPLEMENTATION_LIBRARY_VERSION}"
)

install(
  FILES
  "${wil_nuget_SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-Windows Implementation Library.txt"
)
