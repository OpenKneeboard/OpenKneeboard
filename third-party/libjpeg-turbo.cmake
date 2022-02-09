ExternalProject_Add(
  libjpegTurboBuild
  URL "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/2.1.2.zip"
  URL_HASH "SHA256=46c70b1f098ec794f8a0ed8545d6e82ddb5eea7ca6bdb80476e7039ee2e99637"
  CMAKE_ARGS
    -DENABLE_SHARED=off
    -DWITH_TURBOJPEG=OFF
  INSTALL_COMMAND
    "${CMAKE_COMMAND}" --install . --prefix "<INSTALL_DIR>/$<CONFIG>" --config "$<CONFIG>"
  EXCLUDE_FROM_ALL
)

ExternalProject_Get_property(libjpegTurboBuild INSTALL_DIR)

add_library(libjpegTurbo INTERFACE)
add_dependencies(libjpegTurbo libjpegTurboBuild)
target_link_libraries(libjpegTurbo INTERFACE "${INSTALL_DIR}/$<CONFIG>/lib/jpeg-static.lib")
target_include_directories(libjpegTurbo INTERFACE "${INSTALL_DIR}/$<CONFIG>/include")
