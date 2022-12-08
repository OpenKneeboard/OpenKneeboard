ExternalProject_Add(
  zlibBuild
  URL "https://github.com/madler/zlib/releases/download/v1.2.13/zlib-1.2.13.tar.gz"
  URL_HASH "SHA256=b3a24de97a8fdbc835b9833169501030b8977031bcb54b3b3ac13740f846ab30"
  CMAKE_ARGS
    "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
    "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
  EXCLUDE_FROM_ALL
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
