// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <cinttypes>
#include <ctime>
#include <string_view>

namespace OpenKneeboard::Version {

// clang-format off
static constexpr uint16_t Major = @VERSION_MAJOR@;
static constexpr uint16_t Minor = @VERSION_MINOR@;
static constexpr uint16_t Patch = @VERSION_PATCH@;
static constexpr uint16_t Build = @VERSION_BUILD@;

static constexpr auto ReleaseName {"@RELEASE_NAME@"};
static constexpr auto ReleaseNameW {L"@RELEASE_NAME@"};
static constexpr std::string_view TagName {"@MATCHING_TAG@"};
static constexpr bool IsGithubActionsBuild = @IS_GITHUB_ACTIONS_BUILD@;
static constexpr bool IsTaggedVersion = @IS_TAGGED_VERSION@;
static constexpr bool IsStableRelease = @IS_STABLE_RELEASE@;

static constexpr uint32_t OpenXRApiLayerImplementationVersion = @OPENXR_API_LAYER_IMPLEMENTATION_VERSION@;
// clang-format on

}// namespace OpenKneeboard::Version
