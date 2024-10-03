# Build from source because DCS needs `lua.dll`, not `lua51.dll`
include(FetchContent)

scoped_include(ok_add_library)

FetchContent_Declare(
  lua
  URL "https://www.lua.org/ftp/lua-5.1.5.tar.gz"
  # Using SHA1 as it's listed on the download page
  URL_HASH "SHA1=b3882111ad02ecc6b972f8c1241647905cb2e3fc"
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

FetchContent_GetProperties(lua)

if(NOT lua_POPULATED)
  FetchContent_Populate(lua)

	set(lualib_SOURCE_DIR "${lua_SOURCE_DIR}/src")
	set(
		lualib_SOURCES
		lapi.c lcode.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c
		lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c
		ltable.c ltm.c lundump.c lvm.c lzio.c
		lauxlib.c lbaselib.c ldblib.c liolib.c lmathlib.c loslib.c
		ltablib.c lstrlib.c loadlib.c linit.c)
	list(TRANSFORM lualib_SOURCES PREPEND "${lualib_SOURCE_DIR}/")
	ok_add_library(lualib SHARED RUNTIME_DEPENDENCY EXCLUDE_FROM_ALL ${lualib_SOURCES})
	target_compile_options(lualib PUBLIC "-DLUA_BUILD_AS_DLL")
	target_include_directories(lualib PUBLIC "${lualib_SOURCE_DIR}")
	set_target_properties(lualib
		PROPERTIES
		OUTPUT_NAME lua
		OUTPUT_NAME_Debug luad
	)
endif()

install(
  FILES
  "${lua_SOURCE_DIR}/COPYRIGHT"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-Lua.txt"
)

add_library(ThirdParty::Lua ALIAS lualib)
