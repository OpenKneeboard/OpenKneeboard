/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
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

static constexpr std::string_view ReleaseName {"@RELEASE_NAME@"};
static constexpr std::string_view TagName {"@MATCHING_TAG@"};
static constexpr bool IsGithubActionsBuild = @IS_GITHUB_ACTIONS_BUILD@;
static constexpr bool IsTaggedVersion = @IS_TAGGED_VERSION@;
static constexpr bool IsStableRelease = @IS_STABLE_RELEASE@;
// clang-format on

}// namespace OpenKneeboard::Version
