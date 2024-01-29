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

struct OverlayConfig;

struct OverlayVRPosition {
  enum class Type {
    Absolute,
    // Mirrored in the x = 0 plane, i.e. x => -x
    HorizontalMirror,
  };

  static constexpr OverlayVRPosition Absolute(const VRAbsolutePosition& p) {
    OverlayVRPosition ret;
    ret.mType = Type::Absolute;
    ret.mData = p;
    return ret;
  }

  static constexpr OverlayVRPosition HorizontalMirrorOf(
    const winrt::guid& guid) {
    OverlayVRPosition ret;
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
    const std::vector<OverlayConfig>& others) const;

  constexpr bool operator==(const OverlayVRPosition&) const noexcept = default;

 private:
  OverlayVRPosition() = default;
  Type mType;
  std::variant<winrt::guid, VRAbsolutePosition> mData;
};

struct OverlayNonVRPosition {
  enum class Type {
    Absolute,
    Constrained,
    HorizontalMirror,
    VerticalMirror,
  };

  static constexpr auto Absolute(const NonVRAbsolutePosition& p) {
    OverlayNonVRPosition ret;
    ret.mType = Type::Absolute;
    ret.mData = p;
    return ret;
  }

  static constexpr auto Constrained(const NonVRConstrainedPosition& p) {
    OverlayNonVRPosition ret;
    ret.mType = Type::Constrained;
    ret.mData = p;
    return ret;
  }

  static constexpr auto HorizontalMirrorOf(const winrt::guid& p) {
    OverlayNonVRPosition ret;
    ret.mType = Type::HorizontalMirror;
    ret.mData = p;
    return ret;
  }

  static constexpr auto VerticalMirrorOf(const winrt::guid& p) {
    OverlayNonVRPosition ret;
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

  constexpr bool operator==(const OverlayNonVRPosition&) const noexcept
    = default;

 private:
  OverlayNonVRPosition() = default;
  Type mType;
  std::variant<winrt::guid, NonVRAbsolutePosition, NonVRConstrainedPosition>
    mData;
};

struct OverlayConfig {
  winrt::guid mGuid;
  std::string mName;

  bool mVR {true};
  OverlayVRPosition mVRPosition;

  bool mNonVR {true};
  OverlayNonVRPosition mNonVRPosition;

  static OverlayConfig CreateDefaultFirstOverlay();
  static OverlayConfig CreateDefaultSecondOverlay(const OverlayConfig& first);

  static OverlayConfig CreateRightKnee();
  static OverlayConfig CreateMirroredOverlay(
    std::string_view name,
    const OverlayConfig& other);

  constexpr bool operator==(const OverlayConfig&) const noexcept = default;
};

struct OverlaysConfig {
  OverlaysConfig();
  explicit OverlaysConfig(const nlohmann::json&);

  std::vector<OverlayConfig> mOverlays;

  constexpr bool operator==(const OverlaysConfig&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(OverlaysConfig);

};// namespace OpenKneeboard