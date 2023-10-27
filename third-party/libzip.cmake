set(libzipDeps ThirdParty::ZLib)
add_library(libzipDeps INTERFACE)
target_link_libraries(libzipDeps INTERFACE ${libzipDeps})

ExternalProject_Add(
  libzipBuild
  URL "https://github.com/nih-at/libzip/releases/download/v1.10.1/libzip-1.10.1.tar.gz"
  URL_HASH "SHA256=9669AE5DFE3AC5B3897536DC8466A874C8CF2C0E3B1FDD08D75B273884299363"
  CMAKE_ARGS
    "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
    "-DENABLE_BZIP2=OFF"
    "-DENABLE_LZMA=OFF"
    "-DENABLE_ZSTD=OFF"
    "-DBUILD_SHARED_LIBS=OFF"
    "-DBUILD_TOOLS=OFF"
    "-DBUILD_REGRESS=OFF"
    "-DBUILD_EXAMPLES=OFF"
    "-DBUILD_DOC=OFF"
    "-DZLIB_INCLUDE_DIR=$<TARGET_PROPERTY:zlib,INTERFACE_INCLUDE_DIRECTORIES>"
    "-DZLIB_LIBRARY_DEBUG=$<TARGET_PROPERTY:zlib,INTERFACE_LINK_LIBRARIES_DEBUG>"
    "-DZLIB_LIBRARY_RELEASE=$<TARGET_PROPERTY:zlib,INTERFACE_LINK_LIBRARIES>"
  BUILD_COMMAND
    ${CMAKE_COMMAND} --build . --config "$<CONFIG>" --parallel
  INSTALL_COMMAND
    ${CMAKE_COMMAND} --install . --config "$<CONFIG>" --prefix "<INSTALL_DIR>/$<CONFIG>"
  EXCLUDE_FROM_ALL
  DEPENDS ${libzipDeps}
)
add_dependencies(libzipBuild zlibBuild)

ExternalProject_Get_property(libzipBuild SOURCE_DIR)
ExternalProject_Get_property(libzipBuild INSTALL_DIR)

add_library(libzip INTERFACE)
add_dependencies(libzip libzipBuild)
target_link_libraries(libzip INTERFACE "${INSTALL_DIR}/$<CONFIG>/lib/zip.lib" libzipDeps)
target_include_directories(libzip INTERFACE "${INSTALL_DIR}/$<CONFIG>/include")

add_library(ThirdParty::LibZip ALIAS libzip)

install(
	FILES "${SOURCE_DIR}/LICENSE"
	TYPE DOC
	RENAME "LICENSE-ThirdParty-libzip.txt"
)
