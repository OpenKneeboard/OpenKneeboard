include_guard(GLOBAL)

find_package(GeographicLib CONFIG GLOBAL)

if (NOT GeographicLib_FOUND)
  return()
endif ()

add_library(ThirdParty::GeographicLib ALIAS GeographicLib::GeographicLib)

include(ok_add_license_file)
ok_add_license_file(
  "${OKB_VCPKG_SHARE_DIR}/geographiclib/copyright"
  "LICENSE-ThirdParty-geographiclib.txt"
)
