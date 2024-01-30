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

#include <OpenKneeboard/json.h>
#include <OpenKneeboard/utf8.h>

#include <Windows.h>

#include <algorithm>

namespace OpenKneeboard {

std::optional<SHM::VRQuad> ViewVRPosition::Resolve(
  const PixelSize& contentSize,
  const std::vector<ViewConfig>& others) const {
  if (mType == Type::Empty) {
    return {};
  }

  if (mType == Type::Absolute) {
    auto quad = this->GetQuadConfig();

    using PhysicalSize = Geometry2D::Size<float>;
    const auto physicalSize
      = contentSize.StaticCast<PhysicalSize, float>().ScaledToFit(
        quad.mMaximumPhysicalSize);
    return SHM::VRQuad {quad.mPose, physicalSize};
  }

  winrt::check_bool(mType == Type::HorizontalMirror);

  const auto it = std::ranges::find_if(others, [this](const auto& other) {
    return other.mGuid == GetMirrorOfGUID();
  });
  if (it == others.end()) {
    return {};
  }

  const auto other = it->mVRPosition.Resolve(contentSize, others);
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

NLOHMANN_JSON_SERIALIZE_ENUM(
  ViewVRPosition::Type,
  {
    {ViewVRPosition::Type::Absolute, "Absolute"},
    {ViewVRPosition::Type::Empty, "Empty"},
    {ViewVRPosition::Type::HorizontalMirror, "HorizontalMirror"},
  })

void from_json(const nlohmann::json& j, ViewVRPosition& v) {
  if (!j.contains("Type")) {
    return;
  }

  using Type = ViewVRPosition::Type;
  const Type type = j.at("Type");
  switch (type) {
    case Type::Empty:
      return;
    case Type::Absolute:
      v.SetAbsolute(MaybeGet<VRQuadConfig>(j, "Quad"));
      return;
    case Type::HorizontalMirror:
      v.SetHorizontalMirrorOf(MaybeGet<winrt::guid>(j, "MirrorOf"));
      return;
    default:
      OPENKNEEBOARD_BREAK;
  }
}

void to_json(nlohmann::json& j, const ViewVRPosition& v) {
  j["Type"] = v.GetType();

  MaybeSet(j, "HorizontalAlignment", v.mHorizontalAlignment);
  MaybeSet(j, "VerticalAlignment", v.mVerticalAlignment);

  using Type = ViewVRPosition::Type;
  switch (v.GetType()) {
    case Type::Empty:
      return;
    case Type::Absolute:
      MaybeSet(j, "Quad", v.GetQuadConfig());
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

OPENKNEEBOARD_DEFINE_SPARSE_JSON(
  ViewConfig,
  mGuid,
  mName,
  mVRPosition,
  mNonVRPosition);

OPENKNEEBOARD_DEFINE_SPARSE_JSON(ViewsConfig, mViews);

};// namespace OpenKneeboard