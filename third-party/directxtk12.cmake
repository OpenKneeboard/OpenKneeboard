include(ExternalProject)
ExternalProject_Add(
  directxtk12Build
  URL "https://github.com/microsoft/DirectXTK12/archive/refs/tags/nov2021.zip"
  URL_HASH "SHA256=b2910314e61522998430da0cb88e2452c7826f445ba4bdeaaca0029fedbe08f4"
  BUILD_BYPRODUCTS "<INSTALL_DIR>/$<CONFIG>/lib/DirectXTK12.lib"
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

add_library(directxtk12 INTERFACE)
add_dependencies(directxtk12 directxtk12Build)

ExternalProject_Get_property(directxtk12Build INSTALL_DIR)
target_include_directories(directxtk12 INTERFACE "${INSTALL_DIR}/$<CONFIG>/include")
target_link_libraries(directxtk12 INTERFACE "${INSTALL_DIR}/$<CONFIG>/lib/DirectXTK12.lib")

ExternalProject_Get_property(directxtk12Build SOURCE_DIR)
install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-DirectXTK12.txt"
)

add_library(ThirdParty::DirectXTK12 ALIAS directxtk12)
