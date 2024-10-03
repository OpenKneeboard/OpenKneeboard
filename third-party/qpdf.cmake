include(ExternalProject)

scoped_include(target_include_config_directories)
scoped_include(set_target_config_properties)

scoped_include(libjpeg-turbo.cmake)
scoped_include(zlib.cmake)

# Don't use an INTERFACE library instead: ExternalProject_Add() does not resolve them
set(QPDF_DEPENDENCIES ThirdParty::ZLib ThirdParty::LibJpeg)

# Pull these into CMake variables so we don't need to nest $<GENEX_EVAL> in CMAKE_ARGS
get_target_property(LIBJPEG_LIB ThirdParty::LibJpeg IMPORTED_LOCATION)
get_target_property(ZLIB_LIB ThirdParty::ZLib IMPORTED_LOCATION)

ExternalProject_Add(
  qpdf-build
  URL "https://github.com/qpdf/qpdf/releases/download/v11.6.3/qpdf-11.6.3.tar.gz"
  URL_HASH "SHA256=C394B1B0CFF4CD9D13B0F5E16BDF3CF54DA424DC434F9D40264B7FE67ACD90BC"
  CMAKE_ARGS
  "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
  "-DREQUIRE_CRYPTO_NATIVE=ON"
  "-DUSE_IMPLICIT_CRYPTO=OFF"
  "-DBUILD_SHARED_LIBS=OFF"
  "-DLIBJPEG_H_PATH=$<TARGET_PROPERTY:ThirdParty::LibJpeg,INTERFACE_INCLUDE_DIRECTORIES>"
  "-DLIBJPEG_LIB_PATH=${LIBJPEG_LIB}"
  "-DZLIB_H_PATH=$<TARGET_PROPERTY:ThirdParty::ZLib,INTERFACE_INCLUDE_DIRECTORIES>"
  "-DZLIB_LIB_PATH=${ZLIB_LIB}"
  BUILD_COMMAND
  "${CMAKE_COMMAND}"
  --build .
  --config "$<CONFIG>"
  --target "libqpdf"
  --parallel
  --
  /p:CL_MPCount=
  /p:UseMultiToolTask=true
  /p:EnforceProcessCountAcrossBuilds=true
  INSTALL_COMMAND
  "${CMAKE_COMMAND}"
  --install .
  --prefix "<INSTALL_DIR>/$<CONFIG>"
  --config "$<CONFIG>"
  --component "dev"
  DEPENDS "${QPDF_DEPENDENCIES}"
  INSTALL_BYPRODUCTS "<INSTALL_DIR>/$<CONFIG>/lib/qpdf.lib"

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

ExternalProject_Get_property(qpdf-build SOURCE_DIR)
ExternalProject_Get_property(qpdf-build INSTALL_DIR)
add_library(libqpdf IMPORTED STATIC GLOBAL)
add_dependencies(libqpdf INTERFACE qpdf-build)
target_link_libraries(libqpdf INTERFACE "${QPDF_DEPENDENCIES}")
target_include_config_directories(libqpdf INTERFACE "${INSTALL_DIR}/$<CONFIG>/include")
set_target_config_properties(libqpdf IMPORTED_LOCATION "${INSTALL_DIR}/$<CONFIG>/lib/qpdf.lib")

target_compile_definitions(
  libqpdf
  INTERFACE
  POINTERHOLDER_TRANSITION=4
)
add_library(ThirdParty::QPDF ALIAS libqpdf)

install(
  FILES "${SOURCE_DIR}/NOTICE.md"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-QPDF.txt"
)
