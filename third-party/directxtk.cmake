include(ExternalProject)
# TODO: switch to stable release once 4.x is out (https://github.com/microsoft/Detours/projects/1)
ExternalProject_Add(
  directxtkBuild
  URL "https://github.com/microsoft/DirectXTK/archive/refs/tags/nov2021.zip"
  URL_HASH "SHA256=d25e634b0e225ae572f82d0d27c97051b0069c6813d7be12453039a504dffeb8"
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DBUILD_TOOLS=OFF
    -DBUILD_XAUDIO_WIN10=OFF
    -DBUILD_XAUDIO_WIN8=OFF
    -DBUILD_XAUDIO_WIN7=OFF
)

add_library(directxtk INTERFACE)
add_dependencies(directxtk directxtkBuild)

ExternalProject_Get_property(directxtkBuild INSTALL_DIR)
target_include_directories(directxtk INTERFACE "${INSTALL_DIR}/include")
target_link_libraries(directxtk INTERFACE "${INSTALL_DIR}/lib/DirectXTK.lib")
