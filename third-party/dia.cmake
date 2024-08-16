find_library(
  DIASDK_LIB
  NAMES
  diaguids.lib
  HINTS
  "$ENV{VSINSTALLDIR}/DIA SDK/lib/amd64"
  "${CMAKE_GENERATOR_INSTANCE}/DIA SDK/lib/amd64"
)

find_path(
  DIASDK_INCLUDE_PATH
  dia2.h
  HINTS
  "$ENV{VSINSTALLDIR}/DIA SDK/include"
  "${CMAKE_GENERATOR_INSTANCE}/DIA SDK/include"
)

find_file(
  DIASDK_DLL
  msdia140.dll
  HINTS
  "$ENV{VSINSTALLDIR}/DIA SDK/bin/amd64"
  "${CMAKE_GENERATOR_INSTANCE}/DIA SDK/bin/amd64"
)

add_library(ThirdParty::DIA STATIC IMPORTED GLOBAL)
target_include_directories(ThirdParty::DIA SYSTEM INTERFACE ${DIASDK_INCLUDE_PATH})
set_target_properties(ThirdParty::DIA PROPERTIES IMPORTED_LOCATION ${DIASDK_LIB})