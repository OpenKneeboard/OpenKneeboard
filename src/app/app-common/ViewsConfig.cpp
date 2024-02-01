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
#include <OpenKneeboard/PreferredSize.h>
#include <OpenKneeboard/ViewsConfig.h>

#include <OpenKneeboard/json.h>
#include <OpenKneeboard/utf8.h>

#include <Windows.h>

#include <algorithm>

namespace OpenKneeboard {

std::optional<SHM::VRLayer> ViewVRConfig::Resolve(
  const PreferredSize& contentSize,
  const PixelRect& fullRect,
  const PixelRect& contentRect,
  const std::vector<ViewConfig>& others) const {
  if (mType == Type::Empty) {
    return {};
  }

  if (mType == Type::Independent) {
    auto config = this->GetIndependentConfig();

    auto size = contentSize.mPixelSize.StaticCast<float>().ScaledToFit(
      config.mMaximumPhysicalSize);

    if (contentSize.mPhysicalSize) {
      const auto& ps = *contentSize.mPhysicalSize;
      switch (contentSize.mPhysicalSize->mDirection) {
        case PhysicalSize::Direction::Horizontal: {
          const auto scale = ps.mLength / size.mWidth;
          size.mWidth *= scale;
          size.mHeight *= scale;
          break;
        }
        case PhysicalSize::Direction::Vertical: {
          const auto scale = ps.mLength / size.mHeight;
          size.mWidth *= scale;
          size.mHeight *= scale;
          break;
        }
        case PhysicalSize::Direction::Diagonal: {
          const auto current
            = sqrt((size.mWidth * size.mWidth) + (size.mHeight * size.mHeight));
          const auto scale = ps.mLength / current;
          size.mWidth *= scale;
          size.mHeight *= scale;
          break;
        }
      }
    }

    return SHM::VRLayer {
      .mPose = config.mPose,
      .mPhysicalSize = size,
      .mEnableGazeZoom = config.mEnableGazeZoom,
      .mZoomScale = config.mZoomScale,
      .mGazeTargetScale = config.mGazeTargetScale,
      .mOpacity = config.mOpacity,
      .mLocationOnTexture
      = (config.mDisplayArea == ViewDisplayArea::Full) ? fullRect : contentRect,
    };
  }

  winrt::check_bool(mType == Type::HorizontalMirror);

  const auto it = std::ranges::find(
    others, GetMirrorOfGUID(), [](const auto& it) { return it.mGuid; });
  if (it == others.end()) {
    return {};
  }

  const auto other
    = it->mVR.Resolve(contentSize, fullRect, contentRect, others);
  if (!other) {
    return {};
  }

  auto ret = *other;
  ret.mPose = ret.mPose.GetHorizontalMirror();

  return ret;
}

std::optional<SHM::NonVRLayer> ViewNonVRConfig::Resolve(
  [[maybe_unused]] const PreferredSize& contentSize,
  const PixelRect& fullRect,
  [[maybe_unused]] const PixelRect& contentRect,
  [[maybe_unused]] const std::vector<ViewConfig>& others) const {
  if (!mEnabled) {
    return {};
  }
  return {SHM::NonVRLayer {
    .mPosition = mConstraints,
    .mLocationOnTexture = fullRect,
    .mOpacity = mOpacity,
  }};
}

static void MaybeSet(nlohmann::json& j, std::string_view key, auto value) {
  if (value != decltype(value) {}) {
    j[key] = value;
  }
};

template <class T>
static T MaybeGet(
  const nlohmann::json& j,
  std::string_view key,
  const T& defaultValue = {}) {
  if (j.contains(key)) {
    return j.at(key);
  }
  return defaultValue;
}

NLOHMANN_JSON_SERIALIZE_ENUM(
  ViewDisplayArea,
  {
    {ViewDisplayArea::Full, "Full"},
    {ViewDisplayArea::ContentOnly, "ContentOnly"},
  })

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  IndependentViewVRConfig,
  mPose,
  mMaximumPhysicalSize,
  mEnableGazeZoom,
  mZoomScale,
  mGazeTargetScale,
  mDisplayArea,
  mOpacity)

NLOHMANN_JSON_SERIALIZE_ENUM(
  ViewVRConfig::Type,
  {
    {ViewVRConfig::Type::Independent, "Independent"},
    {ViewVRConfig::Type::Empty, "Empty"},
    {ViewVRConfig::Type::HorizontalMirror, "HorizontalMirror"},
  })

void from_json(const nlohmann::json& j, ViewVRConfig& v) {
  if (!j.contains("Type")) {
    return;
  }

  v.mEnabled = MaybeGet<bool>(j, "Enabled", v.mEnabled);

  using Type = ViewVRConfig::Type;
  const Type type = j.at("Type");
  switch (type) {
    case Type::Empty:
      return;
    case Type::Independent:
      v.SetIndependentConfig(MaybeGet<IndependentViewVRConfig>(j, "Config"));
      return;
    case Type::HorizontalMirror:
      v.SetHorizontalMirrorOf(MaybeGet<winrt::guid>(j, "MirrorOf"));
      return;
    default:
      OPENKNEEBOARD_BREAK;
  }
}

void to_json(nlohmann::json& j, const ViewVRConfig& v) {
  j["Type"] = v.GetType();

  j["Enabled"] = v.mEnabled;

  using Type = ViewVRConfig::Type;
  switch (v.GetType()) {
    case Type::Empty:
      return;
    case Type::Independent:
      MaybeSet(j, "Config", v.GetIndependentConfig());
      return;
    case Type::HorizontalMirror:
      MaybeSet(j, "MirrorOf", v.GetMirrorOfGUID());
      return;
    default:
      OPENKNEEBOARD_BREAK;
  }
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  ViewNonVRConfig,
  mEnabled,
  mConstraints,
  mOpacity);

OPENKNEEBOARD_DEFINE_SPARSE_JSON(ViewConfig, mGuid, mName, mVR, mNonVR);

OPENKNEEBOARD_DEFINE_SPARSE_JSON(ViewsConfig, mViews);
};// namespace OpenKneeboard