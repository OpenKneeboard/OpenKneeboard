ExternalProject_Add(
  zlibBuild
  URL "https://zlib.net/zlib1211.zip"
  URL_HASH "SHA256=d7510a8ee1918b7d0cad197a089c0a2cd4d6df05fee22389f67f115e738b178d"
  CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
  EXCLUDE_FROM_ALL
)

ExternalProject_Get_property(zlibBuild INSTALL_DIR)

add_library(zlib INTERFACE)
add_dependencies(zlib zlibBuild)
target_link_libraries(zlib INTERFACE "${INSTALL_DIR}/lib/zlibstatic$<$<CONFIG:Debug>:d>.lib")
target_include_directories(zlib INTERFACE "${INSTALL_DIR}/include")
