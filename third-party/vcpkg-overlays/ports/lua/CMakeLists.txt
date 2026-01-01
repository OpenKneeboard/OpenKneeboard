CMAKE_MINIMUM_REQUIRED(VERSION 3.25..4.0 FATAL_ERROR)

PROJECT(lua LANGUAGES C)

set(
  SOURCES
  lapi.c lcode.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c
  lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c
  ltable.c ltm.c lundump.c lvm.c lzio.c
  lauxlib.c lbaselib.c ldblib.c liolib.c lmathlib.c loslib.c
  ltablib.c lstrlib.c loadlib.c linit.c)
list(TRANSFORM SOURCES PREPEND "src/")
add_library(lua ${SOURCES})
target_include_directories(
  lua
  PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>"
  PUBLIC "$<INSTALL_INTERFACE:include>"
)
target_compile_features(lua PUBLIC c_std_99)
set_target_properties(lua
  PROPERTIES
  OUTPUT_NAME lua
  OUTPUT_NAME_Debug luad
)

if (BUILD_SHARED_LIBS)
  target_compile_definitions(lua PUBLIC LUA_BUILD_AS_DLL)
endif ()

INSTALL(
  TARGETS lua
  EXPORT openkneeboard-lua-config
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(EXPORT openkneeboard-lua-config DESTINATION share/openkneeboard-lua)

INSTALL(
  FILES
  src/lua.h
  src/lua.h
  src/lualib.h
  src/luaconf.h
  src/lauxlib.h
  DESTINATION include
)