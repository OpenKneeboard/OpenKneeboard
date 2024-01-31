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

std::optional<SHM::VRLayerConfig> ViewVRConfig::Resolve(
  const PreferredSize& contentSize,
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

    return SHM::VRLayerConfig {config.mPose, size};
  }

  winrt::check_bool(mType == Type::HorizontalMirror);

  const auto it = std::ranges::find(
    others, GetMirrorOfGUID(), [](const auto& it) { return it.mGuid; });
  if (it == others.end()) {
    return {};
  }

  const auto other = it->mVR.Resolve(contentSize, others);
  if (!other) {
    return {};
  }

  auto ret = *other;
  ret.mPose.mX = -ret.mPose.mX;
  // Yaw
  ret.mPose.mRY = -ret.mPose.mRY;
  // Roll
  ret.mPose.mRZ = -ret.mPose.mRZ;

  return ret;
}

std::optional<SHM::NonVRPosition> ViewNonVRPosition::Resolve(
  const std::vector<ViewConfig>& others) const {
  if (mType == Type::Empty) {
    return {};
  }
  if (mType == Type::Constrained) {
    return this->GetConstrainedPosition();
  }
  // TODO: implement other types
  return {};
}

ViewsConfig::ViewsConfig() {
}

static void MaybeSet(nlohmann::json& j, std::string_view key, auto value) {
  if (value != decltype(value) {}) {
    j[key] = value;
  }
};

template <class T>
static T MaybeGet(const nlohmann::json& j, std::string_view key) {
  if (j.contains(key)) {
    return j.at(key);
  }
  return {};
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  IndependentViewVRConfig,
  mPose,
  mMaximumPhysicalSize)

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

NLOHMANN_JSON_SERIALIZE_ENUM(
  ViewNonVRPosition::Type,
  {
    {ViewNonVRPosition::Type::Absolute, "Absolute"},
    {ViewNonVRPosition::Type::Constrained, "Constrained"},
    {ViewNonVRPosition::Type::Empty, "Empty"},
    {ViewNonVRPosition::Type::HorizontalMirror, "HorizontalMirror"},
    {ViewNonVRPosition::Type::VerticalMirror, "VerticalMirror"},
  });

void from_json(const nlohmann::json& j, ViewNonVRPosition& v) {
  if (!j.contains("Type")) {
    return;
  }

  using Type = ViewNonVRPosition::Type;
  const Type type = j.at("Type");
  switch (type) {
    case Type::Empty:
      return;
    case Type::Absolute:
      v.SetAbsolute(MaybeGet<NonVRAbsolutePosition>(j, "AbsolutePosition"));
      return;
    case Type::Constrained:
      v.SetConstrained(MaybeGet<NonVRConstrainedPosition>(j, "Constraints"));
      return;
    case Type::HorizontalMirror:
      v.SetHorizontalMirrorOf(j.at("MirrorOf"));
      return;
    case Type::VerticalMirror:
      v.SetVerticalMirrorOf(j.at("MirrorOf"));
      return;
    default:
      OPENKNEEBOARD_BREAK;
  }
}

void to_json(nlohmann::json& j, const ViewNonVRPosition& v) {
  j["Type"] = v.GetType();

  using Type = ViewNonVRPosition::Type;
  switch (v.GetType()) {
    case Type::Empty:
      return;
    case Type::Absolute: {
      MaybeSet(j, "AbsolutePosition", v.GetAbsolutePosition());
      return;
    }
    case Type::Constrained:
      MaybeSet(j, "Constraints", v.GetConstrainedPosition());
      return;
    case Type::HorizontalMirror:
    case Type::VerticalMirror:
      MaybeSet(j, "MirrorOf", v.GetMirrorOfGUID());
      return;
    default:
      OPENKNEEBOARD_BREAK;
  }
}

OPENKNEEBOARD_DEFINE_SPARSE_JSON(ViewNonVRConfig, mPosition)

OPENKNEEBOARD_DEFINE_SPARSE_JSON(ViewConfig, mGuid, mName, mVR, mNonVR);

OPENKNEEBOARD_DEFINE_SPARSE_JSON(ViewsConfig, mViews);
};// namespace OpenKneeboard