find_package(libzip CONFIG GLOBAL)
if (NOT libzip_FOUND)
  return()
endif ()

add_library(ThirdParty::LibZip ALIAS libzip::zip)

include(ok_add_license_file)
ok_add_license_file(
  "${OKB_VCPKG_SHARE_DIR}/libzip/copyright"
  "LICENSE-ThirdParty-libzip.txt"
)
