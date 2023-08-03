include(ExternalProject)
ExternalProject_Add(
  directxtkBuild
  URL "https://github.com/microsoft/DirectXTK/archive/refs/tags/jun2023.zip"
  URL_HASH "SHA256=e5f837edb3d0f8a06471e063b89410b582836ce6b41d5522b03a3448d75d39d9"
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
)

add_library(directxtk INTERFACE)
add_dependencies(directxtk directxtkBuild)

ExternalProject_Get_property(directxtkBuild INSTALL_DIR)
target_include_directories(directxtk INTERFACE "${INSTALL_DIR}/$<CONFIG>/include")
target_link_libraries(directxtk INTERFACE "${INSTALL_DIR}/$<CONFIG>/lib/DirectXTK.lib")

ExternalProject_Get_property(directxtkBuild SOURCE_DIR)
install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-DirectXTK.txt"
)

add_library(ThirdParty::DirectXTK ALIAS directxtk)
