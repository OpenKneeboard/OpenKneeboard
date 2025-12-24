include_guard(GLOBAL)

find_package(detours CONFIG REQUIRED GLOBAL)
add_library(ThirdParty::detours ALIAS Microsoft::Detours)