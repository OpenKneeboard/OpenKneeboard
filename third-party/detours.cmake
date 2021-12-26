include(ExternalProject)
# TODO: switch to stable release once 4.x is out (https://github.com/microsoft/Detours/projects/1)
ExternalProject_Add(
  detoursBuild
  GIT_REPOSITORY https://github.com/microsoft/Detours.git
  BUILD_IN_SOURCE ON
  WORKING_DIRECTORY "<SOURCE_DIR>/src"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND "nmake"
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
)

add_library(detours INTERFACE)
add_dependencies(detours detoursBuild)

ExternalProject_Get_property(detoursBuild SOURCE_DIR)
target_include_directories(detours INTERFACE ${SOURCE_DIR}/include)
target_link_libraries(detours INTERFACE ${SOURCE_DIR}/lib.X64/detours.lib)
