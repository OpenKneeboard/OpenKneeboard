# Copyright (C) 2024 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: MIT
include(FetchContent)

scoped_include("magic_enum.cmake")
scoped_include("json.cmake")

include(FindGit)
find_package(Git REQUIRED)

set(PATCH_FILE "${CMAKE_CURRENT_LIST_DIR}/magic_json_serialize_enum.patch")

FetchContent_Declare(
  magic_json_serialize_enum
  GIT_REPOSITORY "https://github.com/fredemmott/magic_json_serialize_enum"
  GIT_TAG "21287e9ead0adfb1b7af3933acbc0f4edebaaabe"
  GIT_SUBMODULES ""
  EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(magic_json_serialize_enum)

target_link_libraries(
  magic_json_serialize_enum
  INTERFACE
  magic_json_serialize_enum
  ThirdParty::magic_enum
  ThirdParty::JSON
)

add_library(ThirdParty::magic_json_serialize_enum ALIAS magic_json_serialize_enum)

include(ok_add_license_file)
ok_add_license_file(
  "${magic_json_serialize_enum_SOURCE_DIR}/LICENSE"
  "LICENSE-ThirdParty-magic_json_serialize_enum.txt"
)