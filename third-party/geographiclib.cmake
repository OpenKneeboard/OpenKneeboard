include_guard(GLOBAL)

find_package(GeographicLib CONFIG GLOBAL)

if (NOT GeographicLib_FOUND)
  return()
endif ()

add_library(ThirdParty::GeographicLib ALIAS GeographicLib::GeographicLib)