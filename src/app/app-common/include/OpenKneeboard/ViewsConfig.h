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
    Empty,
    Absolute,
    // Mirrored in the x = 0 plane, i.e. x => -x
    HorizontalMirror,
  };

  ViewVRPosition() = default;

  constexpr void SetAbsolute(const VRAbsolutePosition& p) {
    mType = Type::Absolute;
    mData = p;
  }

  constexpr void SetHorizontalMirrorOf(const winrt::guid& guid) {
    mType = Type::HorizontalMirror;
    mData = guid;
  }

  static constexpr auto Absolute(const VRAbsolutePosition& p) {
    ViewVRPosition ret;
    ret.SetAbsolute(p);
    return ret;
  }

  static constexpr auto HorizontalMirrorOf(const winrt::guid& g) {
    ViewVRPosition ret;
    ret.SetHorizontalMirrorOf(g);
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
  Type mType = Type::Empty;
  std::variant<winrt::guid, VRAbsolutePosition> mData;
};

struct ViewNonVRPosition {
  ViewNonVRPosition() = default;

  enum class Type {
    Absolute,
    Constrained,
    Empty,
    HorizontalMirror,
    VerticalMirror,
  };

  constexpr void SetAbsolute(const NonVRAbsolutePosition& p) {
    mType = Type::Absolute;
    mData = p;
  }

  constexpr void SetConstrained(const NonVRConstrainedPosition& p) {
    mType = Type::Constrained;
    mData = p;
  }

  constexpr void SetHorizontalMirrorOf(const winrt::guid& other) {
    mType = Type::HorizontalMirror;
    mData = other;
  }

  constexpr void SetVerticalMirrorOf(const winrt::guid& other) {
    mType = Type::VerticalMirror;
    mData = other;
  }

  static constexpr auto Absolute(const NonVRAbsolutePosition& p) {
    ViewNonVRPosition ret;
    ret.SetAbsolute(p);
    return ret;
  }

  static constexpr auto Constrained(const NonVRConstrainedPosition& p) {
    ViewNonVRPosition ret;
    ret.SetConstrained(p);
    return ret;
  }

  static constexpr auto HorizontalMirrorOf(const winrt::guid& other) {
    ViewNonVRPosition ret;
    ret.SetHorizontalMirrorOf(other);
    return ret;
  }

  static constexpr auto VerticalMirrorOf(const winrt::guid& p) {
    ViewNonVRPosition ret;
    ret.SetVerticalMirrorOf(p);
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

  std::optional<SHM::NonVRPosition> Resolve(
    const std::vector<ViewConfig>& others) const;

 private:
  Type mType {Type::Empty};
  std::variant<winrt::guid, NonVRAbsolutePosition, NonVRConstrainedPosition>
    mData;
};

struct ViewConfig {
  winrt::guid mGuid = random_guid();
  std::string mName;

  ViewVRPosition mVRPosition;
  ViewNonVRPosition mNonVRPosition;

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