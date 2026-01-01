# Output filenames use underscores rather than hyphens for
# increased compatibility; in particular, LUA requires the DLL
# name matches a C function name.

ok_add_library(
  OpenKneeboard-lua-api
  MODULE
  DUALARCH
  OUTPUT_NAME "OpenKneeboard_LuaAPI"
  OpenKneeboard_LuaAPI.cpp)
target_link_libraries(OpenKneeboard-lua-api PUBLIC ThirdParty::Lua)
target_link_libraries(
  OpenKneeboard-lua-api
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-tracing
  OpenKneeboard-APIEvent)

ok_add_library(
  OpenKneeboard-c-api
  MODULE
  DUALARCH
  OUTPUT_NAME "OpenKneeboard_CAPI"
  OpenKneeboard_CAPI.cpp)
target_link_libraries(
  OpenKneeboard-c-api
  PRIVATE
  OpenKneeboard-dprint
  OpenKneeboard-tracing
  OpenKneeboard-APIEvent)
target_include_directories(
  OpenKneeboard-c-api
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR})
file(
  GENERATE
  OUTPUT
  "${BUILD_OUT_ROOT}/include/OpenKneeboard_CAPI.h"
  INPUT
  "OpenKneeboard_CAPI.h"
)
