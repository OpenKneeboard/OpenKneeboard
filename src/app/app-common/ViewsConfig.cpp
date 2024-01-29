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
#include <OpenKneeboard/ViewsConfig.h>

#include <OpenKneeboard/utf8.h>

#include <Windows.h>

#include <algorithm>

static winrt::guid CreateGUID() {
  winrt::guid ret;
  winrt::check_hresult(CoCreateGuid(reinterpret_cast<GUID*>(&ret)));
  return ret;
}

namespace OpenKneeboard {

std::optional<SHM::VRPosition> ViewVRPosition::Resolve(
  const std::vector<ViewConfig>& others) const {
  if (mType == Type::Absolute) {
    // FIXME: adjust for alignment and size
    return GetAbsolutePosition();
  }

  winrt::check_bool(mType == Type::HorizontalMirror);

  const auto it = std::ranges::find_if(others, [this](const auto& other) {
    return other.mGuid == GetMirrorOfGUID();
  });
  if (it == others.end()) {
    return {};
  }

  const auto other = it->mVRPosition.Resolve(others);
  if (!other) {
    return {};
  }

  auto ret = *other;
  ret.mX = -ret.mX;
  // Yaw
  ret.mRY = -ret.mRY;
  // Roll
  ret.mRZ = -ret.mRZ;

  return ret;
}

ViewConfig ViewConfig::CreateDefaultFirstView() {
  return ViewConfig::CreateRightKnee();
}

ViewConfig ViewConfig::CreateRightKnee() {
  return ViewConfig {
    .mGuid = CreateGUID(),
    .mName = _("Right Kneeboard"),
    .mVRPosition = ViewVRPosition::Absolute({}),
    .mNonVRPosition = ViewNonVRPosition::Constrained({}),
  };
}

ViewConfig ViewConfig::CreateDefaultSecondView(const ViewConfig& first) {
  return CreateMirroredView(_("Left Kneeboard"), first);
}

ViewConfig ViewConfig::CreateMirroredView(
  std::string_view name,
  const ViewConfig& other) {
  return ViewConfig {
    .mGuid = CreateGUID(),
    .mName = std::string {name},
    .mVRPosition = ViewVRPosition::HorizontalMirrorOf(other.mGuid),
    .mNonVRPosition = ViewNonVRPosition::HorizontalMirrorOf(other.mGuid),
  };
}

ViewsConfig::ViewsConfig() {
}

};// namespace OpenKneeboard