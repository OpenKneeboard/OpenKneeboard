# Copyright (C) 2024 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: MIT

set(OPENKNEEBOARD_THIRD_PARTY_WEAK_REFS_PATH OFF CACHE PATH "Set to a local checkout of the weak_refs project")
if(OPENKNEEBOARD_THIRD_PARTY_WEAK_REFS_PATH)
    set(use_path_default ON)
else()
    set(use_path_default OFF)
endif()
OPTION(USE_OPENKNEEBOARD_THIRD_PARTY_WEAK_REFS_PATH "Use the OPENKNEEBOARD_THIRD_PARTY_WEAK_REFS_PATH variable instead of FetchContent()" ${use_path_default})

if (USE_OPENKNEEBOARD_THIRD_PARTY_WEAK_REFS_PATH)
    message(WARNING "⚠️ Using local checkout of FredEmmott/weak_refs from `${OPENKNEEBOARD_THIRD_PARTY_WEAK_REFS_PATH}`")
    add_subdirectory(
        ${OPENKNEEBOARD_THIRD_PARTY_WEAK_REFS_PATH}
        ${CMAKE_CURRENT_BINARY_DIR}/weak_refs
        EXCLUDE_FROM_ALL
    )
else()
    include(FetchContent)

    FetchContent_Declare(
        weak_refs
        GIT_REPOSITORY "https://github.com/fredemmott/weak_refs.git"
        GIT_TAG "5bfcbd347f653a29142fa493708abeb24ee82700"
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(weak_refs)
endif()

add_library(weak_refs_unified INTERFACE)
target_link_libraries(
    weak_refs_unified
    INTERFACE
    FredEmmott::composable_bind
    FredEmmott::cppwinrt
    FredEmmott::weak_refs
    ThirdParty::CppWinRT
    ThirdParty::WIL
)

add_library(ThirdParty::weak_refs ALIAS weak_refs_unified)

install(
    FILES
    "${weak_refs_unified_SOURCE_DIR}/LICENSE"
  TYPE DOC
  RENAME "LICENSE-ThirdParty-weak_refs.txt"
)