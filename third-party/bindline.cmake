# Copyright (C) 2024 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: MIT

include(FetchContent)

FetchContent_Declare(
    bindline
    GIT_REPOSITORY "https://github.com/fredemmott/bindline.git"
    GIT_TAG "e773c57ace691d449265af61101220f71160fce8"
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(bindline)

add_library(bindline_unified INTERFACE)
target_link_libraries(
    bindline_unified
    INTERFACE
    FredEmmott::bindline
    FredEmmott::cppwinrt
    FredEmmott::weak_refs
    ThirdParty::CppWinRT
    ThirdParty::WIL
)

add_library(ThirdParty::bindline ALIAS bindline_unified)

install(
    FILES
    "${bindline_SOURCE_DIR}/LICENSE"
    TYPE DOC
    RENAME "LICENSE-ThirdParty-bindline.txt"
)