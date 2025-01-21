include(ExternalProject)
ExternalProject_Add(
  directxtk12Build
  URL "https://github.com/microsoft/DirectXTK12/archive/refs/tags/oct2024.zip"
  URL_HASH "SHA256=f2393ef53584b2c1b29d4a734ef7c2a5acd356d8c38d97c1ea98a78ca452ded2"
  BUILD_BYPRODUCTS "<INSTALL_DIR>/$<CONFIG>/lib/DirectXTK12.lib"
  CMAKE_ARGS
  "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
  -DBUILD_XAUDIO_WIN10=OFF
  -DDIRECTX_ARCH="$<IF:$<EQUAL:${BUILD_BITNESS},64>,x64,x86>"

  # Split the install dir by configuration so we don't have mismatches for ITERATOR_DEBUG_LEVEL
  # or MSVCRT
  INSTALL_COMMAND
  ${CMAKE_COMMAND} --install . "--prefix=<INSTALL_DIR>/$<CONFIG>" --config "$<CONFIG>"
  
  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  STEP_TARGETS update
)

add_library(directxtk12 INTERFACE)
add_dependencies(directxtk12 directxtk12Build)

ExternalProject_Get_property(directxtk12Build INSTALL_DIR)
target_include_directories(directxtk12 INTERFACE "${INSTALL_DIR}/$<CONFIG>/include")
target_link_libraries(directxtk12 INTERFACE "${INSTALL_DIR}/$<CONFIG>/lib/DirectXTK12.lib")

ExternalProject_Get_property(directxtk12Build SOURCE_DIR)

add_library(ThirdParty::DirectXTK12 ALIAS directxtk12)


include(ok_add_license_file)
ok_add_license_file("${SOURCE_DIR}/LICENSE" "LICENSE-ThirdParty-DirectXTK12.txt" DEPENDS directxtk12Build-update)