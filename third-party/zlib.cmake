ExternalProject_Add(
  zlibBuild
  URL "https://github.com/madler/zlib/releases/download/v1.3/zlib-1.3.tar.gz"
  URL_HASH "SHA256=FF0BA4C292013DBC27530B3A81E1F9A813CD39DE01CA5E0F8BF355702EFA593E"
  CMAKE_ARGS
    "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
    "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

ExternalProject_Get_property(zlibBuild INSTALL_DIR)

add_library(zlib INTERFACE)
add_dependencies(zlib zlibBuild)
target_link_libraries(zlib INTERFACE "${INSTALL_DIR}/lib/zlibstatic$<$<CONFIG:Debug>:d>.lib")
target_include_directories(zlib INTERFACE "${INSTALL_DIR}/include")

add_library(ThirdParty::ZLib ALIAS zlib)

install(
	FILES "${CMAKE_CURRENT_LIST_DIR}/zlib.COPYING.txt"
	TYPE DOC
	RENAME "LICENSE-ThirdParty-zlib.txt"
)
