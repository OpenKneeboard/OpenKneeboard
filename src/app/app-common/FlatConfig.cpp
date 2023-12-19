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
#include <OpenKneeboard/FlatConfig.h>

#include <OpenKneeboard/json.h>

namespace OpenKneeboard {

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  NonVRConstrainedPosition,
  mHeightPercent,
  mPaddingPixels,
  mOpacity,
  mHorizontalAlignment,
  mVerticalAlignment)

void to_json(nlohmann::json& j, const NonVRAbsolutePosition& p) {
  const auto& r = p.mRect;
  j["Origin"] = {
    {"Left", r.mOrigin.mX},
    {"Top", r.mOrigin.mY},
  };
  j["Size"] = {
    {"Width", r.mSize.mWidth},
    {"Height", r.mSize.mHeight},
  };
  j["Alignment"] = {
    {"Horizontal", p.mHorizontalAlignment},
    {"Vertical", p.mVerticalAlignment},
  };
}// namespace OpenKneeboard

void from_json(const nlohmann::json& j, NonVRAbsolutePosition& p) {
  const auto origin = j.at("Origin");
  const auto size = j.at("Size");
  const auto alignment = j.at("Alignment");

  auto& r = p.mRect;
  r.mOrigin.mX = origin.at("Left");
  r.mOrigin.mY = origin.at("Top");
  r.mSize.mWidth = size.at("Width");
  r.mSize.mHeight = size.at("Height");
  p.mHorizontalAlignment = alignment.at("Horizontal");
  p.mVerticalAlignment = alignment.at("Vertical");
}

}// namespace OpenKneeboard
