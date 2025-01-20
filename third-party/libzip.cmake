include(ExternalProject)

scoped_include("zlib.cmake")
scoped_include(target_include_config_directories)
scoped_include(set_target_config_properties)

# Don't use an INTERFACE library instead: ExternalProject_Add() does not resolve them
set(LIBZIP_DEPENDENCIES ThirdParty::ZLib)

ExternalProject_Add(
  libzip-ep
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
    "-DZLIB_LIBRARY_DEBUG=$<TARGET_PROPERTY:zlib,IMPORTED_LOCATION_DEBUG>"
    "-DZLIB_LIBRARY_RELEASE=$<TARGET_PROPERTY:zlib,IMPORTED_LOCATION_RELEASE>"
  BUILD_COMMAND
    "${CMAKE_COMMAND}" --build . --config "$<CONFIG>" --parallel
  INSTALL_COMMAND
    "${CMAKE_COMMAND}" --install . --config "$<CONFIG>" --prefix "<INSTALL_DIR>/$<CONFIG>"
  INSTALL_BYPRODUCTS
    "<INSTALL_DIR>/$<CONFIG>/lib/zip.lib"
    "<INSTALL_DIR>/$<CONFIG>/include"
  DEPENDS ${LIBZIP_DEPENDENCIES}

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  STEP_TARGETS update
)

ExternalProject_Get_property(libzip-ep INSTALL_DIR SOURCE_DIR)

add_library(libzip IMPORTED STATIC GLOBAL)
add_dependencies(libzip libzip-ep)
target_link_libraries(libzip INTERFACE ${LIBZIP_DEPENDENCIES})
set_target_config_properties(libzip IMPORTED_LOCATION "${INSTALL_DIR}/$<CONFIG>/lib/zip.lib")
target_include_config_directories(libzip INTERFACE "${INSTALL_DIR}/$<CONFIG>/include")

add_library(ThirdParty::LibZip ALIAS libzip)

include(ok_add_license_file)
ok_add_license_file(
	"${SOURCE_DIR}/LICENSE"
	"LICENSE-ThirdParty-libzip.txt"
  DEPENDS libzip-ep-update
)
