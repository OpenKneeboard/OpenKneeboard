include_guard(GLOBAL)
find_package(felly CONFIG REQUIRED GLOBAL)
add_library(ThirdParty::felly ALIAS felly::felly)