include(ExternalProject)
# TODO: switch to stable release once 4.x is out (https://github.com/microsoft/Detours/projects/1)
ExternalProject_Add(
  detoursBuild
  GIT_REPOSITORY https://github.com/microsoft/Detours.git
  BUILD_IN_SOURCE ON
  WORKING_DIRECTORY "<SOURCE_DIR>/src"
  UPDATE_DISCONNECTED ON
  CONFIGURE_COMMAND ""
  BUILD_COMMAND
    ${CMAKE_COMMAND} -E env
    "DETOURS_CONFIG=$<IF:$<CONFIG:Debug>,Debug,Release>"
    nmake
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
)

add_library(detours INTERFACE)
add_dependencies(detours detoursBuild)

ExternalProject_Get_property(detoursBuild SOURCE_DIR)
target_include_directories(detours INTERFACE ${SOURCE_DIR}/include)
target_link_libraries(detours INTERFACE "${SOURCE_DIR}/lib.X64$<$<CONFIG:Debug>:Debug>/detours.lib")
