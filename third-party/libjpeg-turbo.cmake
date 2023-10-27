ExternalProject_Add(
  libjpegTurboBuild
  URL "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/3.0.1.zip"
  URL_HASH "SHA256=D6D99E693366BC03897677650E8B2DFA76B5D6C54E2C9E70C03F0AF821B0A52F"
  CMAKE_ARGS
    "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
    -DENABLE_SHARED=off
    -DWITH_TURBOJPEG=OFF
  INSTALL_COMMAND
    "${CMAKE_COMMAND}" --install . --prefix "<INSTALL_DIR>/$<CONFIG>" --config "$<CONFIG>"
  EXCLUDE_FROM_ALL
)

ExternalProject_Get_property(libjpegTurboBuild SOURCE_DIR)
ExternalProject_Get_property(libjpegTurboBuild INSTALL_DIR)

add_library(libjpegTurbo INTERFACE)
add_dependencies(libjpegTurbo libjpegTurboBuild)
target_link_libraries(libjpegTurbo INTERFACE "${INSTALL_DIR}/$<CONFIG>/lib/jpeg-static.lib")
target_include_directories(libjpegTurbo INTERFACE "${INSTALL_DIR}/$<CONFIG>/include")

add_library(ThirdParty::LibJpeg ALIAS libjpegTurbo)

install(
	FILES "${SOURCE_DIR}/LICENSE.md"
	TYPE DOC
	RENAME "LICENSE-ThirdParty-libjpeg-turbo.txt"
)
