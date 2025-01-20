include_guard(GLOBAL)

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
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  STEP_TARGETS update
)

add_library(json INTERFACE)
add_dependencies(json jsonFetch)

ExternalProject_Get_property(jsonFetch INSTALL_DIR)
target_include_directories(json SYSTEM INTERFACE "${INSTALL_DIR}/include")
target_compile_definitions(json INTERFACE JSON_DISABLE_ENUM_SERIALIZATION=1)

ExternalProject_Get_property(jsonFetch SOURCE_DIR)

add_library(ThirdParty::JSON ALIAS json)
include(ok_add_license_file)
ok_add_license_file("${SOURCE_DIR}/LICENSE.MIT" "LICENSE-ThirdParty-JSON for Modern CPP.txt" DEPENDS jsonFetch-update)