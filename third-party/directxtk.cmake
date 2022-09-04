include(ExternalProject)
ExternalProject_Add(
  directxtkBuild
  URL "https://github.com/microsoft/DirectXTK/archive/refs/tags/jun2022.zip"
  URL_HASH "SHA256=347515b8d769e83b4f88dd8cefebd5afb489c932579ba6fdf9e2cf2692412dfb"
  BUILD_BYPRODUCTS "<INSTALL_DIR>/$<CONFIG>/lib/DirectXTK.lib"
  CMAKE_ARGS
    "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
    -DBUILD_TOOLS=OFF
    -DBUILD_XAUDIO_WIN10=OFF
    -DBUILD_XAUDIO_WIN8=OFF
    -DBUILD_XAUDIO_WIN7=OFF
  BUILD_COMMAND
    ${CMAKE_COMMAND}
    --build .
    --config "$<CONFIG>"
    --parallel
    --
    /p:CL_MPCount=
    /p:UseMultiToolTask=true
    /p:EnforceProcessCountAcrossBuilds=true
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
