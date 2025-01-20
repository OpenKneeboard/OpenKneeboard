# Copyright (C) 2024 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: MIT
include_guard(GLOBAL)

include(FetchContent)

FetchContent_Declare(
    magic_enum
    URL "https://github.com/Neargye/magic_enum/releases/download/v0.9.6/magic_enum-v0.9.6.tar.gz"
    URL_HASH "SHA256=83C8367F1FF738A32D4D904C46CEE31C643E766848F5112D77DA4A737B1EDD0B"
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(magic_enum)

add_library(tp_magic_enum INTERFACE)
target_link_libraries(tp_magic_enum INTERFACE magic_enum::magic_enum)
add_library(ThirdParty::magic_enum ALIAS tp_magic_enum)

include(ok_add_license_file)
ok_add_license_file(
  "${magic_enum_SOURCE_DIR}/LICENSE"
  "LICENSE-ThirdParty-magic_enum.txt"
)