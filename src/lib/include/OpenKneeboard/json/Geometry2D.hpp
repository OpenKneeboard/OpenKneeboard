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