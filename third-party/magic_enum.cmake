include_guard(GLOBAL)

find_package(magic_enum CONFIG GLOBAL REQUIRED)

add_library(ThirdParty::magic_enum ALIAS magic_enum::magic_enum)

include(ok_add_license_file)
ok_add_license_file(
  "${OKB_VCPKG_SHARE_DIR}/magic-enum/copyright"
  "LICENSE-ThirdParty-magic_enum.txt"
)