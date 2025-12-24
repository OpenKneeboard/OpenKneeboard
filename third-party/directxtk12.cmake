include_guard(GLOBAL)
find_package(directxtk12 CONFIG REQUIRED GLOBAL)
add_library(ThirdParty::DirectXTK12 ALIAS Microsoft::DirectXTK12)