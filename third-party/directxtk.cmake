include_guard(GLOBAL)
find_package(directxtk CONFIG REQUIRED GLOBAL)
add_library(ThirdParty::DirectXTK ALIAS Microsoft::DirectXTK)