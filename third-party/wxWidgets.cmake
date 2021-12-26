include(FetchContent)

FetchContent_Declare(
  wxwidgets
  URL "https://github.com/wxWidgets/wxWidgets/releases/download/v3.1.5/wxWidgets-3.1.5.zip"
  # Using SHA1 as wxWidgets list it on the release notes
  URL_HASH "SHA1=69e4dcf9be7cad0261ee5874064a8747a5585aaa"
)

FetchContent_GetProperties(wxwidgets)

if(NOT wxwidgets_POPULATED)
  FetchContent_Populate(wxwidgets)

  set(wxBUILD_SHARED OFF)
  add_subdirectory(${wxwidgets_SOURCE_DIR} ${wxwidgets_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
