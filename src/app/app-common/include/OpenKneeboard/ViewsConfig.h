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

#include <OpenKneeboard/FlatConfig.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/VRConfig.h>

#include <OpenKneeboard/json_fwd.h>

#include <shims/winrt/base.h>

#include <variant>

namespace OpenKneeboard {

struct ViewConfig;

struct ViewVRPosition {
  enum class Type {
    Absolute,
    // Mirrored in the x = 0 plane, i.e. x => -x
    HorizontalMirror,
  };

  static constexpr ViewVRPosition Absolute(const VRAbsolutePosition& p) {
    ViewVRPosition ret;
    ret.mType = Type::Absolute;
    ret.mData = p;
    return ret;
  }

  static constexpr ViewVRPosition HorizontalMirrorOf(const winrt::guid& guid) {
    ViewVRPosition ret;
    ret.mType = Type::HorizontalMirror;
    ret.mData = guid;
    return ret;
  }

  constexpr Type GetType() const noexcept {
    return mType;
  }

  constexpr auto GetMirrorOfGUID() const noexcept {
    return std::get<winrt::guid>(mData);
  }

  constexpr auto GetAbsolutePosition() const noexcept {
    return std::get<VRAbsolutePosition>(mData);
  }

  Alignment::Horizontal mHorizontalAlignment {Alignment::Horizontal::Center};
  Alignment::Vertical mVerticalAlignment {Alignment::Vertical::Middle};

  std::optional<SHM::VRPosition> Resolve(
    const std::vector<ViewConfig>& others) const;

  constexpr bool operator==(const ViewVRPosition&) const noexcept = default;

 private:
  ViewVRPosition() = default;
  Type mType;
  std::variant<winrt::guid, VRAbsolutePosition> mData;
};

struct ViewNonVRPosition {
  enum class Type {
    Absolute,
    Constrained,
    HorizontalMirror,
    VerticalMirror,
  };

  static constexpr auto Absolute(const NonVRAbsolutePosition& p) {
    ViewNonVRPosition ret;
    ret.mType = Type::Absolute;
    ret.mData = p;
    return ret;
  }

  static constexpr auto Constrained(const NonVRConstrainedPosition& p) {
    ViewNonVRPosition ret;
    ret.mType = Type::Constrained;
    ret.mData = p;
    return ret;
  }

  static constexpr auto HorizontalMirrorOf(const winrt::guid& p) {
    ViewNonVRPosition ret;
    ret.mType = Type::HorizontalMirror;
    ret.mData = p;
    return ret;
  }

  static constexpr auto VerticalMirrorOf(const winrt::guid& p) {
    ViewNonVRPosition ret;
    ret.mType = Type::VerticalMirror;
    ret.mData = p;
    return ret;
  }

  constexpr Type GetType() const noexcept {
    return mType;
  }

  constexpr auto GetMirrorOfGUID() const noexcept {
    return std::get<winrt::guid>(mData);
  }

  constexpr auto GetAbsolutePosition() const noexcept {
    return std::get<NonVRAbsolutePosition>(mData);
  }

  constexpr auto GetConstrainedPosition() const noexcept {
    return std::get<NonVRConstrainedPosition>(mData);
  }

  constexpr bool operator==(const ViewNonVRPosition&) const noexcept = default;

 private:
  ViewNonVRPosition() = default;
  Type mType;
  std::variant<winrt::guid, NonVRAbsolutePosition, NonVRConstrainedPosition>
    mData;
};

struct ViewConfig {
  winrt::guid mGuid;
  std::string mName;

  bool mVR {true};
  ViewVRPosition mVRPosition;

  bool mNonVR {true};
  ViewNonVRPosition mNonVRPosition;

  static ViewConfig CreateDefaultFirstView();
  static ViewConfig CreateDefaultSecondView(const ViewConfig& first);

  static ViewConfig CreateRightKnee();
  static ViewConfig CreateMirroredView(
    std::string_view name,
    const ViewConfig& other);

  constexpr bool operator==(const ViewConfig&) const noexcept = default;
};

struct ViewsConfig {
  ViewsConfig();
  explicit ViewsConfig(const nlohmann::json&);

  std::vector<ViewConfig> mViews;

  constexpr bool operator==(const ViewsConfig&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(ViewsConfig);

};// namespace OpenKneeboard