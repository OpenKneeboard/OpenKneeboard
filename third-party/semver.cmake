include(ExternalProject)
ExternalProject_Add(
  semverBuild
  URL "https://github.com/zmarko/semver/archive/refs/tags/1.1.0.zip"
  URL_HASH "SHA256=f6d9b40d5be853adf878148b630a49e833227c53da639da6ea43028f2b1c2588"
  BUILD_BYPRODUCTS "<BINARY_DIR>/src/$<CONFIG>/semver.lib"
  CMAKE_ARGS
    "-DCMAKE_TOOLCHAIN_FILE=${THIRDPARTY_TOOLCHAIN_FILE}"
    "-DBUILD_SHARED_LIBS=Off"
  INSTALL_COMMAND
    ${CMAKE_COMMAND} --install . "--prefix=<INSTALL_DIR>/$<CONFIG>" --config "$<CONFIG>"
  EXCLUDE_FROM_ALL
)

add_library(semver INTERFACE)
add_dependencies(semver semverBuild)

ExternalProject_Get_property(semverBuild SOURCE_DIR)
ExternalProject_Get_property(semverBuild BINARY_DIR)
target_include_directories(semver INTERFACE "${SOURCE_DIR}/include")
target_link_libraries(semver INTERFACE "${BINARY_DIR}/src/$<CONFIG>/semver.lib")

install(
  FILES
  "${SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-semver.txt"
)

add_library(ThirdParty::SemVer ALIAS semver)
