add_library(detours INTERFACE)
target_include_directories(detours INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Detours/include)
target_link_libraries(detours INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Detours/lib.X64/detours.lib)

add_library(oculus-sdk-headers INTERFACE)
target_include_directories(oculus-sdk-headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ovr_sdk_win_32.0.0/LibOVR/Include)
#target_link_libraries(oculus-sdk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ovr_sdk_win_32.0.0/LibOVR/Lib/Windows/x64/Release/VS2017/LibOVR.lib)

add_library(fmt INTERFACE)
target_include_directories(fmt INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/fmt-8.0.1/include)
target_compile_definitions(fmt INTERFACE "-DFMT_HEADER_ONLY")

set(wxBUILD_SHARED OFF)
add_subdirectory(wxWidgets-3.1.5 EXCLUDE_FROM_ALL)

set(LUALIB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lua-5.1.5/src")
set(
  LUALIB_SOURCES
  lapi.c lcode.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c
  lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c
  ltable.c ltm.c lundump.c lvm.c lzio.c
  lauxlib.c lbaselib.c ldblib.c liolib.c lmathlib.c loslib.c
  ltablib.c lstrlib.c loadlib.c linit.c)
list(TRANSFORM LUALIB_SOURCES PREPEND "${LUALIB_SOURCE_DIR}/")

add_library(lualib SHARED EXCLUDE_FROM_ALL ${LUALIB_SOURCES})
target_compile_options(lualib PUBLIC "-DLUA_BUILD_AS_DLL")
target_include_directories(lualib PUBLIC "${LUALIB_SOURCE_DIR}")
set_target_properties(lualib
  PROPERTIES
  OUTPUT_NAME lua
  OUTPUT_NAME_Debug luad
)
