// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Geometry2D.hpp>

#include <OpenKneeboard/json.hpp>

namespace OpenKneeboard::Geometry2D {

template <class T>
void from_json(const nlohmann::json& j, Size<T>& v) {
  v.mWidth = j.at("Width");
  v.mHeight = j.at("Height");
}
template <class T>
void to_json(nlohmann::json& j, const Size<T>& v) {
  j["Width"] = v.mWidth;
  j["Height"] = v.mHeight;
}

}