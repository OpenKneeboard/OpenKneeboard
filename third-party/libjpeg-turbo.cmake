include_guard(GLOBAL)
scoped_include(target_include_config_directories)
scoped_include(set_target_config_properties)

ExternalProject_Add(
  libjpeg-turbo-build
  URL "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/3.0.1.zip"
  URL_HASH "SHA256=D6D99E693366BC03897677650E8B2DFA76B5D6C54E2C9E70C03F0AF821B0A52F"
  CMAKE_ARGS
    "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    -DENABLE_SHARED=off
    -DWITH_TURBOJPEG=OFF
  INSTALL_COMMAND
    "${CMAKE_COMMAND}" --install . --prefix "<INSTALL_DIR>/$<CONFIG>" --config "$<CONFIG>"

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  STEP_TARGETS update
)

ExternalProject_Get_property(libjpeg-turbo-build SOURCE_DIR)
ExternalProject_Get_property(libjpeg-turbo-build INSTALL_DIR)

set(LIBJPEG_INCLUDE_DIR "${INSTALL_DIR}/$<CONFIG>/include")
scoped_include(make_config_directories)
make_config_directories("${LIBJPEG_INCLUDE_DIR}")

add_library(libjpeg-turbo IMPORTED STATIC GLOBAL)
add_dependencies(libjpeg-turbo libjpeg-turbo-build)
target_include_config_directories(libjpeg-turbo INTERFACE "${INSTALL_DIR}/$<CONFIG>/include")
set_target_config_properties(libjpeg-turbo IMPORTED_LOCATION "${INSTALL_DIR}/$<CONFIG>/lib/jpeg-static.lib")

add_library(ThirdParty::LibJpeg ALIAS libjpeg-turbo)

include(ok_add_license_file)
ok_add_license_file(
	"${SOURCE_DIR}/LICENSE.md"
	"LICENSE-ThirdParty-libjpeg-turbo.txt"
  DEPENDS libjpeg-turbo-build-update
)
