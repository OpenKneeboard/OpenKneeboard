include_guard(GLOBAL)

find_package(nlohmann_json CONFIG REQUIRED GLOBAL)

add_library(json_without_enums INTERFACE)
target_link_libraries(json_without_enums INTERFACE nlohmann_json::nlohmann_json)
target_compile_definitions(json_without_enums INTERFACE JSON_DISABLE_ENUM_SERIALIZATION=1)

add_library(ThirdParty::JSON ALIAS json_without_enums)