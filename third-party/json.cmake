ExternalProject_Add(
  jsonFetch
  URL "https://github.com/nlohmann/json/releases/download/v3.10.4/json.hpp"
  URL_HASH "SHA256=c9ac7589260f36ea7016d4d51a6c95809803298c7caec9f55830a0214c5f9140"
  DOWNLOAD_NO_EXTRACT ON
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND
  ${CMAKE_COMMAND} -E copy
  "<DOWNLOADED_FILE>" "<INSTALL_DIR>"
  EXCLUDE_FROM_ALL
)

add_library(json INTERFACE)
add_dependencies(json jsonFetch)

ExternalProject_Get_property(jsonFetch INSTALL_DIR)
target_include_directories(json INTERFACE "${INSTALL_DIR}")
