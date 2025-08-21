ExternalProject_Add(
  geographiclibBuild
  URL "https://github.com/geographiclib/geographiclib/archive/refs/tags/v2.3.zip"
  URL_HASH "SHA256=8954e2e0db0a2ff24a40bee1b9424aed5f84dcfbad671f3783b6aab6103cf226"
  CMAKE_ARGS
  "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
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
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  STEP_TARGETS update
)

ExternalProject_Get_property(geographiclibBuild SOURCE_DIR)
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

include(ok_add_license_file)
ok_add_license_file("${SOURCE_DIR}/LICENSE.txt" "LICENSE-ThirdParty-geographiclib.txt" DEPENDS geographiclibBuild-update)
