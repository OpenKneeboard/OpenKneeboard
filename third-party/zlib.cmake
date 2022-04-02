ExternalProject_Add(
  zlibBuild
  URL "https://zlib.net/fossils/zlib-1.2.12.tar.gz"
  URL_HASH "SHA256=91844808532e5ce316b3c010929493c0244f3d37593afd6de04f71821d5136d9"
  CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
  EXCLUDE_FROM_ALL
)

ExternalProject_Get_property(zlibBuild INSTALL_DIR)

add_library(zlib INTERFACE)
add_dependencies(zlib zlibBuild)
target_link_libraries(zlib INTERFACE "${INSTALL_DIR}/lib/zlibstatic$<$<CONFIG:Debug>:d>.lib")
target_include_directories(zlib INTERFACE "${INSTALL_DIR}/include")

add_library(ThirdParty::ZLib ALIAS zlib)
