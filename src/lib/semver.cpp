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
#include <OpenKneeboard/semver.hpp>

#include <regex>

#include <semver200.h>

namespace OpenKneeboard {
std::string ToSemVerString(std::string_view raw) {
  // Remove leading v
  std::string ret {(raw.front() == 'v') ? raw.substr(1) : raw};
  // The .z in x.y.z is mandatory
  ret = std::regex_replace(
    ret,
    std::regex {"^(\\d+\\.\\d+)(-|$)"},
    "\\1.0\\2",
    std::regex_constants::format_sed);
  // 'beta3' should be 'beta.3' to get numeric comparisons
  ret = std::regex_replace(
    ret,
    std::regex {"-([a-z]+)(\\d+)\\b"},
    "-\\1.\\2",
    std::regex_constants::format_sed);
  return ret;
}

ThreeWayCompareResult CompareSemVer(std::string_view as, std::string_view bs) {
  const version::Semver200_version a {std::string {as}};
  const version::Semver200_version b {std::string {bs}};

  using enum ThreeWayCompareResult;
  if (a < b) {
    return LessThan;
  }
  if (a > b) {
    return GreaterThan;
  }
  return Equal;
}

ThreeWayCompareResult CompareVersions(std::string_view a, std::string_view b) {
  return CompareSemVer(ToSemVerString(a), ToSemVerString(b));
}

}// namespace OpenKneeboard