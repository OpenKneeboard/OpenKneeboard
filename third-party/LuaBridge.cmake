ExternalProject_Add(
  LuaBridgeFetch
  URL "https://github.com/vinniefalco/LuaBridge/archive/refs/tags/2.6.zip"
  URL_HASH "SHA256=b1cc56b9b8c61e29e69580848beaaf6c7d0a5972f890dd93c667033f08dd7f07"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  EXCLUDE_FROM_ALL
)

add_library(LuaBridge INTERFACE)
add_dependencies(LuaBridge LuaBridgeFetch)

ExternalProject_Get_property(LuaBridgeFetch SOURCE_DIR)
target_include_directories(LuaBridge INTERFACE "${SOURCE_DIR}/Source")
target_link_libraries(LuaBridge INTERFACE ThirdParty::Lua)

add_library(ThirdParty::LuaBridge ALIAS LuaBridge)
