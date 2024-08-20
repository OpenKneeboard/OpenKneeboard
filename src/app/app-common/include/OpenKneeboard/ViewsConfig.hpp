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

#include <OpenKneeboard/FlatConfig.hpp>
#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/VRConfig.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/bitflags.hpp>
#include <OpenKneeboard/format/enum.hpp>
#include <OpenKneeboard/json_fwd.hpp>

#include <variant>

namespace OpenKneeboard {

enum class ViewDisplayArea {
  Full,
  ContentOnly,
};

enum class ResolveViewFlags : uint8_t {
  Default = 0,
  IncludeDisabled = 1,
};
template <>
constexpr bool is_bitflags_v<ResolveViewFlags> = true;

struct ViewConfig;
struct PreferredSize;

/// Configuration of a VR view that is not a mirror of another.
struct IndependentViewVRConfig {
  VRPose mPose;
  Geometry2D::Size<float> mMaximumPhysicalSize {0.15f, 0.25f};

  bool mEnableGazeZoom {true};
  float mZoomScale = 2.0f;
  GazeTargetScale mGazeTargetScale {};
  VROpacityConfig mOpacity {};
  ViewDisplayArea mDisplayArea {ViewDisplayArea::Full};

  constexpr bool operator==(const IndependentViewVRConfig&) const noexcept
    = default;
};

/** VR configuration of a view.
 *
 * This might be an 'independent' view, in which case it has its'
 * own `IndependentViewVRConfig`, or a mirror of another, in which
 * case it just stores the GUID of the view it's mirroring.
 */
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
      fatal("Can't get an independent view for {:#}", mType);
    }
    return std::get<IndependentViewVRConfig>(mData);
  }

  constexpr auto SetIndependentConfig(const IndependentViewVRConfig& v) {
    mType = Type::Independent;
    mData = v;
  }

  constexpr auto GetMirrorOfGUID() const {
    if (mType != Type::HorizontalMirror) {
      fatal("Can't get a mirror GUID for {:#}", mType);
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

  std::optional<SHM::VRLayer> Resolve(
    const PreferredSize& preferredSize,
    const PixelRect& fullRect,
    const PixelRect& contentRect,
    const std::vector<ViewConfig>& others,
    ResolveViewFlags flags = ResolveViewFlags::Default) const;

  constexpr bool operator==(const ViewVRConfig&) const noexcept = default;

 private:
  Type mType {Type::Empty};
  std::variant<IndependentViewVRConfig, winrt::guid> mData;
};

/// Non-VR configuration of a view
struct ViewNonVRConfig {
  bool mEnabled {false};
  NonVRConstrainedPosition mConstraints;
  float mOpacity = 0.8f;

  constexpr bool operator==(const ViewNonVRConfig&) const noexcept = default;

  std::optional<SHM::NonVRLayer> Resolve(
    const PreferredSize& contentSize,
    const PixelRect& fullRect,
    const PixelRect& contentRect,
    const std::vector<ViewConfig>& others,
    ResolveViewFlags flags = ResolveViewFlags::Default) const;
};

struct ViewConfig {
  winrt::guid mGuid = random_guid();
  std::string mName;

  ViewVRConfig mVR;
  ViewNonVRConfig mNonVR;

  winrt::guid mDefaultTabID;

  bool operator==(const ViewConfig&) const noexcept = default;
};

enum class AppWindowViewMode {
  /** The user hasn't been asked yet; they should be prompted
   * when they add a second view, or on next startup if they already
   * have two.
   */
  NoDecision,

  /** The main window shows the active KneeboardView.
   *
   * Changing tabs/pages affects the in-game view.
   */
  ActiveView,

  /**
   * The main window has its' own KneeboardView.
   *
   * Changing tabs/pages does not affect the in-game view.
   */
  Independent,
};

struct ViewsConfig {
  std::vector<ViewConfig> mViews;

  AppWindowViewMode mAppWindowMode {AppWindowViewMode::NoDecision};

  constexpr bool operator==(const ViewsConfig&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(ViewsConfig);
};// namespace OpenKneeboard