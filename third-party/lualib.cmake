find_package(openkneeboard-lua CONFIG REQUIRED GLOBAL)

add_library(ThirdParty::Lua ALIAS lua)
add_library(lualib_delayload INTERFACE)
target_link_libraries(lualib_delayload INTERFACE lua)
target_link_options(lualib_delayload INTERFACE "/DELAYLOAD:$<TARGET_FILE_NAME:lua>")
add_library(ThirdParty::Lua::DelayLoad ALIAS lualib_delayload)

get_target_property(
  LUA_RELEASE_DLL
  lua
  IMPORTED_LOCATION_RELEASE
)
get_target_property(
  LUA_DEBUG_DLL
  lua
  IMPORTED_LOCATION_DEBUG
)

include(ok_add_runtime_files)
ok_add_runtime_files(
  lua-runtime-dll
  "$<IF:$<CONFIG:Debug>,${LUA_DEBUG_DLL},${LUA_RELEASE_DLL}>"
)