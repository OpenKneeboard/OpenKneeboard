ExternalProject_Add(
  jsonFetch
  URL "https://github.com/nlohmann/json/releases/download/v3.10.4/include.zip"
  URL_HASH "SHA256=62c585468054e2d8e7c2759c0d990fd339d13be988577699366fe195162d16cb"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND
  ${CMAKE_COMMAND} -E copy_directory
  "<SOURCE_DIR>/include" "<INSTALL_DIR>/include"

  EXCLUDE_FROM_ALL
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
  RENAME "LICENSE-ThirdParty-JSON_for_Modern_CPP"
)
