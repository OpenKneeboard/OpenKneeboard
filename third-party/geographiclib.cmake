ExternalProject_Add(
  geographiclibBuild
  URL "https://github.com/geographiclib/geographiclib/archive/refs/tags/v2.1.zip"
  URL_HASH "SHA256=a03fbd13434622a32de90098fa8781857639ea0f7e4aaa6905304e4083c6e512"
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

