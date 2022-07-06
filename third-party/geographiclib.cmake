ExternalProject_Add(
  geographiclibBuild
  URL "https://sourceforge.net/projects/geographiclib/files/distrib-C%2B%2B/GeographicLib-2.1.zip/download"
  URL_HASH "SHA256=519e88e0c07a2067a633870e973af90100f9fbde1429ae6738a3805d06ce55b1"
  CMAKE_ARGS
    "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
    -DBUILD_SHARED_LIBS=OFF
  BUILD_COMMAND
    "${CMAKE_COMMAND}"
    --build .
    --config "$<CONFIG>"
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
  EXCLUDE_FROM_ALL
)

ExternalProject_Get_property(geographiclibBuild INSTALL_DIR)
add_library(geographiclib INTERFACE)
add_dependencies(geographiclib INTERFACE geographiclibBuild)

target_link_libraries(
	geographiclib
	INTERFACE
	"${INSTALL_DIR}/$<CONFIG>/lib/geographiclib$<$<CONFIG:Debug>:_d>.lib"
)
target_include_directories(
	geographiclib
	INTERFACE
	"${INSTALL_DIR}/$<CONFIG>/include"
)
add_library(ThirdParty::GeographicLib ALIAS geographiclib)

