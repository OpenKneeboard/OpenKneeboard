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
struct PreferredSize;

struct IndependentViewVRConfig {
  VRPose mPose;
  Geometry2D::Size<float> mMaximumPhysicalSize;

  constexpr bool operator==(const IndependentViewVRConfig&) const noexcept
    = default;
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

struct ViewVRConfig {
  enum class Type {
    Empty,
    Independent,
    HorizontalMirror,
  };

  bool mEnabled {true};

  constexpr Type GetType() const {
    return mType;
  }

  constexpr auto GetIndependentConfig() const {
    if (mType != Type::Independent) [[unlikely]] {
      OPENKNEEBOARD_FATAL;
    }
    return std::get<IndependentViewVRConfig>(mData);
  }

  constexpr auto SetIndependentConfig(const IndependentViewVRConfig& v) {
    mType = Type::Independent;
    mData = v;
  }

  constexpr auto GetMirrorOfGUID() const {
    if (mType != Type::HorizontalMirror) {
      OPENKNEEBOARD_FATAL;
    }
    return std::get<winrt::guid>(mData);
  }

  constexpr auto SetHorizontalMirrorOf(const winrt::guid& v) {
    mType = Type::HorizontalMirror;
    mData = v;
  }

  static ViewVRConfig Independent(const IndependentViewVRConfig& v) {
    ViewVRConfig ret;
    ret.SetIndependentConfig(v);
    return ret;
  };

  static ViewVRConfig HorizontalMirrorOf(const winrt::guid& v) {
    ViewVRConfig ret;
    ret.SetHorizontalMirrorOf(v);
    return ret;
  };

  std::optional<SHM::VRLayerConfig> Resolve(
    const PreferredSize& contentSize,
    const std::vector<ViewConfig>& others) const;

  constexpr bool operator==(const ViewVRConfig&) const noexcept = default;

 private:
  Type mType {Type::Empty};
  std::variant<IndependentViewVRConfig, winrt::guid> mData;
};

struct ViewNonVRConfig {
  ViewNonVRPosition mPosition;
  constexpr bool operator==(const ViewNonVRConfig&) const noexcept = default;
};

struct ViewConfig {
  winrt::guid mGuid = random_guid();
  std::string mName;

  ViewVRConfig mVR;
  ViewNonVRConfig mNonVR;

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