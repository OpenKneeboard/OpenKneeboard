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

extern const uint16_t Major;
extern const uint16_t Minor;
extern const uint16_t Patch;
extern const uint16_t Build;

extern const std::string_view CommitIDA;
extern const std::wstring_view CommitIDW;
extern const std::string_view ReleaseName;
// Didn't see a clean way to get this formatted by CMake as UTC :(
extern const std::time_t CommitUnixTimestamp;
extern const char* ModifiedFiles;
extern const bool HaveModifiedFiles;
extern const char* BuildTimestamp;
extern const bool IsGithubActionsBuild;

}// namespace OpenKneeboard::Version
