ExternalProject_Add(
  jsonFetch
  URL "https://github.com/nlohmann/json/releases/download/v3.11.2/include.zip"
  URL_HASH "SHA256=e5c7a9f49a16814be27e4ed0ee900ecd0092bfb7dbfca65b5a421b774dccaaed"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND
  ${CMAKE_COMMAND} -E copy_directory
  "<SOURCE_DIR>/include" "<INSTALL_DIR>/include"

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

add_library(json INTERFACE)
add_dependencies(json jsonFetch)

ExternalProject_Get_property(jsonFetch INSTALL_DIR)
target_include_directories(json INTERFACE "${INSTALL_DIR}/include")


ExternalProject_Get_property(jsonFetch SOURCE_DIR)
install(
  FILES
  "${SOURCE_DIR}/LICENSE.MIT"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-JSON for Modern CPP.txt"
)

add_library(ThirdParty::JSON ALIAS json)
