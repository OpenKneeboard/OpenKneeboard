include_guard(GLOBAL)

find_package(rectangle-bin-pack CONFIG REQUIRED GLOBAL)
add_library(ThirdParty::RectangleBinPack ALIAS RectangleBinPack::RectangleBinPack)