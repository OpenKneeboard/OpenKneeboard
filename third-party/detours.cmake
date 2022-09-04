set(DETOURS_CONFIG "$<IF:$<STREQUAL:$<CONFIG>,Debug>,Debug,Release>")
set(LIB "lib.X64${DETOURS_CONFIG}/detours.lib")
include(ExternalProject)

# TODO: switch to stable release once 4.x is out (https://github.com/microsoft/Detours/projects/1)
ExternalProject_Add(
  detoursBuild
  GIT_REPOSITORY https://github.com/microsoft/Detours.git
  BUILD_IN_SOURCE ON
  WORKING_DIRECTORY "<SOURCE_DIR>/src"
  UPDATE_DISCONNECTED ON
  BUILD_BYPRODUCTS "<SOURCE_DIR>/${LIB}"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND
    ${CMAKE_COMMAND} -E env
    "DETOURS_CONFIG=${DETOURS_CONFIG}"
    nmake
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
)

add_library(detours INTERFACE)
add_dependencies(detours detoursBuild)

ExternalProject_Get_property(detoursBuild SOURCE_DIR)
target_include_directories(detours INTERFACE ${SOURCE_DIR}/include)
target_link_libraries(detours INTERFACE "${SOURCE_DIR}/${LIB}")

install(
  FILES
  "${SOURCE_DIR}/LICENSE.md"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-Detours.txt"
)
