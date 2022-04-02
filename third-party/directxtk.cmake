include(ExternalProject)
ExternalProject_Add(
  directxtkBuild
  URL "https://github.com/microsoft/DirectXTK/archive/refs/tags/mar2022.zip"
  URL_HASH "SHA256=18dda50a26efabc8c0aa509255beb29f534449a86d1db72c06465e8fb5b66ad1"
  BUILD_BYPRODUCTS "<INSTALL_DIR>/$<CONFIG>/lib/DirectXTK.lib"
  CMAKE_ARGS
    -DBUILD_TOOLS=OFF
    -DBUILD_XAUDIO_WIN10=OFF
    -DBUILD_XAUDIO_WIN8=OFF
    -DBUILD_XAUDIO_WIN7=OFF
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
  RENAME "LICENSE-ThirdParty-DirectXTK"
)

add_library(ThirdParty::DirectXTK ALIAS directxtk)
