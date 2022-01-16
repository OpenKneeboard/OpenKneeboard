set(LIB "lib.X64Release/detours.lib")
include(ExternalProject)

# TODO: switch to stable release once 4.x is out (https://github.com/microsoft/Detours/projects/1)
#
# We are always using a release build as otherwise DETOURS_TRACE calls printf, which can deadlock if
# a slept thread is using malloc/delete/new/free

set(NO_NEW_OR_DELETE_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/detours-0001-eliminates-possible-deadlock-related-to-memory-alloc.patch")
ExternalProject_Add(
  detoursBuild
  GIT_REPOSITORY https://github.com/microsoft/Detours.git
  BUILD_IN_SOURCE ON
  WORKING_DIRECTORY "<SOURCE_DIR>/src"
  UPDATE_DISCONNECTED ON
  BUILD_BYPRODUCTS "<SOURCE_DIR>/${LIB}"
  PATCH_COMMAND
    git apply --reverse --check "${NO_NEW_OR_DELETE_PATCH}"
      || git am "${NO_NEW_OR_DELETE_PATCH}"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND
    ${CMAKE_COMMAND} -E env
    "DETOURS_CONFIG=Release"
    nmake
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
)

add_library(detours INTERFACE)
add_dependencies(detours detoursBuild)

ExternalProject_Get_property(detoursBuild SOURCE_DIR)
target_include_directories(detours INTERFACE ${SOURCE_DIR}/include)
target_link_libraries(detours INTERFACE "${SOURCE_DIR}/${LIB}")
