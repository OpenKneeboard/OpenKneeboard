ExternalProject_Add(
  eigen3Build
  URL "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip"
  URL_HASH "SHA256=1ccaabbfe870f60af3d6a519c53e09f3dcf630207321dffa553564a8e75c4fc8"

  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
)

add_library(eigen3 INTERFACE)
add_dependencies(eigen3 qvmBuild)

ExternalProject_Get_property(eigen3Build SOURCE_DIR)

target_include_directories(eigen3 INTERFACE "${SOURCE_DIR}")
