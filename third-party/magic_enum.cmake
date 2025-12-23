include_guard(GLOBAL)

find_package(magic_enum CONFIG GLOBAL REQUIRED)

add_library(ThirdParty::magic_enum ALIAS magic_enum::magic_enum)