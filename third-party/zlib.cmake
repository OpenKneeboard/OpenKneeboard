include_guard(GLOBAL)

include(set_target_config_properties)

ExternalProject_Add(
  zlib-ep
  URL "https://github.com/madler/zlib/releases/download/v1.3/zlib-1.3.tar.gz"
  URL_HASH "SHA256=FF0BA4C292013DBC27530B3A81E1F9A813CD39DE01CA5E0F8BF355702EFA593E"
  CMAKE_ARGS
    "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
    "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
  BUILD_COMMAND
  "${CMAKE_COMMAND}"
  --build .
  --config "$<CONFIG>"
  --target zlibstatic
  INSTALL_COMMAND ""
  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

ExternalProject_Get_property(zlib-ep BINARY_DIR SOURCE_DIR)

add_library(zlib IMPORTED STATIC GLOBAL)
add_dependencies(zlib zlib-ep)

target_include_directories(zlib INTERFACE "${SOURCE_DIR}" "${BINARY_DIR}")
set_target_config_properties(zlib IMPORTED_LOCATION "${BINARY_DIR}/$<CONFIG>/zlibstatic.lib")
set_target_properties(
  zlib
  PROPERTIES
  # Add the `d` suffix
  IMPORTED_LOCATION_DEBUG "${BINARY_DIR}/Debug/zlibstaticd.lib"
  # For other stuff to pass to ExternalProject args
  IMPORTED_LOCATION "${BINARY_DIR}/$<CONFIG>/zlibstatic$<$<CONFIG:Debug>:d>.lib"
)

add_library(ThirdParty::ZLib ALIAS zlib)

include(ok_add_license_file)
ok_add_license_file(
	"${CMAKE_CURRENT_LIST_DIR}/zlib.COPYING.txt"
	"LICENSE-ThirdParty-zlib.txt"
)
