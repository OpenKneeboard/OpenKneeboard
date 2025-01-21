include(ExternalProject)
ExternalProject_Add(
  directxtkBuild
  URL "https://github.com/microsoft/DirectXTK/archive/refs/tags/dec2023.zip"
  URL_HASH "SHA256=592e88df64fd49977699b6afafc0129e23ec1f8871450e202eb59605d557dd34"
  BUILD_BYPRODUCTS "<INSTALL_DIR>/$<CONFIG>/lib/DirectXTK.lib"
  CMAKE_ARGS
  "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
  -DBUILD_TOOLS=OFF
  -DBUILD_XAUDIO_WIN10=OFF
  -DBUILD_XAUDIO_WIN8=OFF
  -DBUILD_XAUDIO_WIN7=OFF
  -DDIRECTX_ARCH="$<IF:$<EQUAL:${BUILD_BITNESS},64>,x64,x86>"

  # Split the install dir by configuration so we don't have mismatches for ITERATOR_DEBUG_LEVEL
  # or MSVCRT
  INSTALL_COMMAND
  ${CMAKE_COMMAND} --install . "--prefix=<INSTALL_DIR>/$<CONFIG>" --config "$<CONFIG>"
  
  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  STEP_TARGETS update
)

add_library(directxtk INTERFACE)
add_dependencies(directxtk directxtkBuild)

ExternalProject_Get_property(directxtkBuild INSTALL_DIR)
target_include_directories(directxtk INTERFACE "${INSTALL_DIR}/$<CONFIG>/include")
target_link_libraries(directxtk INTERFACE "${INSTALL_DIR}/$<CONFIG>/lib/DirectXTK.lib")

ExternalProject_Get_property(directxtkBuild SOURCE_DIR)

add_library(ThirdParty::DirectXTK ALIAS directxtk)

include(ok_add_license_file)
ok_add_license_file("${SOURCE_DIR}/LICENSE" "LICENSE-ThirdParty-DirectXTK.txt" DEPENDS directxtkBuild-update)