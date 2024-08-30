ExternalProject_Add(
  jsonFetch
  URL "https://github.com/nlohmann/json/releases/download/v3.11.3/include.zip"
  URL_HASH "SHA256=a22461d13119ac5c78f205d3df1db13403e58ce1bb1794edc9313677313f4a9d"

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
target_include_directories(json SYSTEM INTERFACE "${INSTALL_DIR}/include")
target_compile_definitions(json INTERFACE JSON_DISABLE_ENUM_SERIALIZATION 1)

ExternalProject_Get_property(jsonFetch SOURCE_DIR)
install(
  FILES
  "${SOURCE_DIR}/LICENSE.MIT"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-JSON for Modern CPP.txt"
)

add_library(ThirdParty::JSON ALIAS json)
