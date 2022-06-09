ExternalProject_Add(
  qpdfBuild
  URL "https://github.com/qpdf/qpdf/releases/download/release-qpdf-10.6.3.0cmake1/qpdf-10.6.3.0cmake1.tar.gz"
  URL_HASH "SHA256=6a431eb49ed013de9e373e92bd6f90bcb75c31a7929633b113b223d7e081c857"
  CMAKE_ARGS
    "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
    "-DREQUIRE_CRYPTO_NATIVE=ON"
    "-DUSE_IMPLICIT_CRYPTO=OFF"
    "-DBUILD_SHARED_LIBS=OFF"
    "-DLIBJPEG_H_PATH=$<TARGET_PROPERTY:ThirdParty::LibJpeg,INTERFACE_INCLUDE_DIRECTORIES>"
    "-DLIBJPEG_LIB_PATH=$<TARGET_PROPERTY:ThirdParty::LibJpeg,INTERFACE_LINK_LIBRARIES>"
    "-DZLIB_H_PATH=$<TARGET_PROPERTY:ThirdParty::ZLib,INTERFACE_INCLUDE_DIRECTORIES>"
    "-DZLIB_LIB_PATH=$<TARGET_PROPERTY:ThirdParty::ZLib,INTERFACE_LINK_LIBRARIES>"
  BUILD_COMMAND
    "${CMAKE_COMMAND}"
    --build .
    --config "$<CONFIG>"
    --target "libqpdf"
    --parallel
  INSTALL_COMMAND
    "${CMAKE_COMMAND}"
    --install .
    --prefix "<INSTALL_DIR>/$<CONFIG>"
    --config "$<CONFIG>"
    --component "dev"
  EXCLUDE_FROM_ALL
)

add_dependencies(
  qpdfBuild
  ThirdParty::LibJpeg
  ThirdParty::ZLib
)

ExternalProject_Get_property(qpdfBuild SOURCE_DIR)
ExternalProject_Get_property(qpdfBuild INSTALL_DIR)
add_library(libqpdf INTERFACE)
add_dependencies(libqpdf INTERFACE qpdfBuild)

target_link_libraries(
  libqpdf
  INTERFACE
  "${INSTALL_DIR}/$<CONFIG>/lib/qpdf.lib"
)
target_include_directories(
  libqpdf
  INTERFACE
  "${INSTALL_DIR}/$<CONFIG>/include"
)
add_library(ThirdParty::QPDF ALIAS libqpdf)
