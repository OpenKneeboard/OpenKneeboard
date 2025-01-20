set(DETOURS_CONFIG "$<IF:$<STREQUAL:$<CONFIG>,Debug>,Debug,Release>")
include(ExternalProject)

if(BUILD_BITNESS EQUAL 32)
  set(DETOURS_TARGET_PROCESSOR x86)
  set(LIB "lib.X86${DETOURS_CONFIG}/detours.lib")
else()
  set(DETOURS_TARGET_PROCESSOR x64)
  set(LIB "lib.X64${DETOURS_CONFIG}/detours.lib")
endif()

# TODO: switch to stable release once 4.x is out (https://github.com/microsoft/Detours/projects/1)
set(DETOURS_GIT_REV 4b8c659f549b0ab21cf649377c7a84eb708f5e68)
find_package(Git)
ExternalProject_Add(
  detoursBuild
  GIT_REPOSITORY https://github.com/microsoft/Detours.git
  GIT_TAG main
  BUILD_IN_SOURCE ON
  UPDATE_DISCONNECTED ON
  BUILD_BYPRODUCTS "<SOURCE_DIR>/${LIB}"
  UPDATE_COMMAND
  "${GIT_EXECUTABLE}" reset --hard "${DETOURS_GIT_REV}"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND
  "${CMAKE_COMMAND}" -E chdir
  "<SOURCE_DIR>/src"
  "${CMAKE_COMMAND}" -E env
  "DETOURS_CONFIG=${DETOURS_CONFIG}"
  "DETOURS_TARGET_PROCESSOR=${DETOURS_TARGET_PROCESSOR}"
  nmake
  INSTALL_COMMAND ""

  EXCLUDE_FROM_ALL
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  STEP_TARGETS update
)

add_library(detours INTERFACE)
add_dependencies(detours detoursBuild)

ExternalProject_Get_property(detoursBuild SOURCE_DIR)
target_include_directories(detours INTERFACE ${SOURCE_DIR}/include)
target_link_libraries(detours INTERFACE "${SOURCE_DIR}/${LIB}")

add_library(ThirdParty::detours ALIAS detours)

include(ok_add_license_file)
ok_add_license_file("${SOURCE_DIR}/LICENSE.md" "LICENSE-ThirdParty-Detours.txt" DEPENDS detoursBuild-update)
