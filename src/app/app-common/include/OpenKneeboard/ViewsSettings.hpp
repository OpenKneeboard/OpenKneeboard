// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/VRSettings.hpp>

#include <OpenKneeboard/bitflags.hpp>
#include <OpenKneeboard/format/enum.hpp>
#include <OpenKneeboard/json_fwd.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <shims/winrt/base.h>

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

struct ViewSettings;
struct PreferredSize;

/// Configuration of a VR view that is not a mirror of another.
struct IndependentViewVRSettings {
  VRPose mPose;
  Geometry2D::Size<float> mMaximumPhysicalSize {0.15f, 0.25f};

  bool mEnableGazeZoom {true};
  float mZoomScale = 2.0f;
  GazeTargetScale mGazeTargetScale {};
  VROpacitySettings mOpacity {};
  ViewDisplayArea mDisplayArea {ViewDisplayArea::Full};

  constexpr bool operator==(const IndependentViewVRSettings&) const noexcept
    = default;
};

/** VR configuration of a view.
 *
 * This might be an 'independent' view, in which case it has its'
 * own `IndependentViewVRSettings`, or a mirror of another, in which
 * case it just stores the GUID of the view it's mirroring.
 */
struct ViewVRSettings {
  enum class Type {
    Empty,
    Independent,
    HorizontalMirror,
  };

  bool mEnabled {true};

  constexpr Type GetType() const {
    return mType;
  }

  constexpr auto GetIndependentSettings() const {
    if (mType != Type::Independent) [[unlikely]] {
      fatal("Can't get an independent view for {:#}", mType);
    }
    return std::get<IndependentViewVRSettings>(mData);
  }

  constexpr auto SetIndependentSettings(const IndependentViewVRSettings& v) {
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

  static ViewVRSettings Independent(const IndependentViewVRSettings& v) {
    ViewVRSettings ret;
    ret.SetIndependentSettings(v);
    return ret;
  };

  static ViewVRSettings HorizontalMirrorOf(const winrt::guid& v) {
    ViewVRSettings ret;
    ret.SetHorizontalMirrorOf(v);
    return ret;
  };

  std::optional<SHM::VRLayer> Resolve(
    const PreferredSize& preferredSize,
    const PixelRect& fullRect,
    const PixelRect& contentRect,
    const std::vector<ViewSettings>& others,
    ResolveViewFlags flags = ResolveViewFlags::Default) const;

  constexpr bool operator==(const ViewVRSettings&) const noexcept = default;

 private:
  Type mType {Type::Empty};
  std::variant<IndependentViewVRSettings, winrt::guid> mData;
};

struct ViewSettings {
  winrt::guid mGuid = random_guid();
  std::string mName;

  ViewVRSettings mVR;

  winrt::guid mDefaultTabID;

  bool operator==(const ViewSettings&) const noexcept = default;
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

struct ViewsSettings {
  std::vector<ViewSettings> mViews {
    ViewSettings {
      .mName = _("Kneeboard 1"),
      .mVR = ViewVRSettings::Independent({}),
    },
  };

  AppWindowViewMode mAppWindowMode {AppWindowViewMode::NoDecision};

  constexpr bool operator==(const ViewsSettings&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(ViewsSettings);
};// namespace OpenKneeboard