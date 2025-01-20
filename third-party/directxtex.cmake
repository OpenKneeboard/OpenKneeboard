include(ExternalProject)
ExternalProject_Add(
  directxtexEP
  URL "https://github.com/microsoft/DirectXTex/archive/refs/tags/dec2023.zip"
  URL_HASH "SHA256=5f1332bb76ef5c9e8403f2c854c25e67d8c84fa42dc238e62ebadc62bca4287c"
  BUILD_BYPRODUCTS "<INSTALL_DIR>/$<CONFIG>/lib/DirectXTex.lib"
  CMAKE_ARGS
  "-DBUILD_TOOLS=OFF"
  "-DBUILD_SAMPLE=OFF"
  "-DBUILD_DX12=OFF"
  "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
  -DDIRECTX_ARCH="$<IF:$<EQUAL:${BUILD_BITNESS},64>,x64,x86>"

  # Split the install dir by configuration so we don't have mismatches for ITERATOR_DEBUG_LEVEL
  # or MSVCRT
  INSTALL_COMMAND
  ${CMAKE_COMMAND} --install . "--prefix=<INSTALL_DIR>/$<CONFIG>" --config "$<CONFIG>"
  
  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  STEP_TARGETS update
)

add_library(directxtex INTERFACE)
add_dependencies(directxtex directxtexEP)

ExternalProject_Get_property(directxtexEP INSTALL_DIR)
target_include_directories(directxtex INTERFACE "${INSTALL_DIR}/$<CONFIG>/include")
target_link_libraries(directxtex INTERFACE "${INSTALL_DIR}/$<CONFIG>/lib/DirectXTex.lib")

ExternalProject_Get_property(directxtexEP SOURCE_DIR)

add_library(ThirdParty::DirectXTex ALIAS directxtex)

include(ok_add_license_file)
ok_add_license_file("${SOURCE_DIR}/LICENSE" "LICENSE-ThirdParty-DirectXTex.txt" DEPENDS directxtexEP-update)