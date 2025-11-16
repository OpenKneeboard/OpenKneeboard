// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <string>

namespace OpenKneeboard {

enum class ThreeWayCompareResult {
  LessThan = -1,
  Equal = 0,
  GreaterThan = 1,
};

/// Best-effort; works for OpenKneeboard conventions, maybe nothing else
std::string ToSemVerString(std::string_view);

ThreeWayCompareResult CompareSemVer(std::string_view, std::string_view);
/// Best-effort: use ToSemVerString() them CompareSemVer()
ThreeWayCompareResult CompareVersions(std::string_view, std::string_view);

}// namespace OpenKneeboard