include(ExternalProject)
ExternalProject_Add(
  directxtkBuild
  URL "https://github.com/microsoft/DirectXTK/archive/refs/tags/nov2021.zip"
  URL_HASH "SHA256=d25e634b0e225ae572f82d0d27c97051b0069c6813d7be12453039a504dffeb8"
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
