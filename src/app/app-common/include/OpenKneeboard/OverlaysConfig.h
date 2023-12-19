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
#include <OpenKneeboard/VRConfig.h>

#include <shims/winrt/base.h>

namespace OpenKneeboard {

struct OverlayVRPosition {
  enum class Type {
    Absolute,
    HorizontalMirror,
    VerticalMirror,
  };

  Type mType;
  union {
    winrt::guid mMirrorOf;
    VRAbsolutePosition mAbsolutePosition;
  };
};

struct OverlayNonVRPosition {
  enum class Type {
    Absolute,
    Constrained,
    HorizontalMirror,
    VerticalMirror,
  };

  Type mType;
  union {
    winrt::guid mMirrorOf;
    NonVRAbsolutePosition mAbsolutePosition;
    NonVRConstrainedPosition mConstrainedPosition;
  };
};

struct OverlayConfig {
  winrt::guid mGuid;
  std::string mName;

  bool mVR {true};
  OverlayVRPosition mVRPosition;

  bool mNonVR {true};
  OverlayNonVRPosition mNonVRPosition;
};

struct OverlaysConfig {
  std::vector<OverlayConfig> mOverlays;
};

};// namespace OpenKneeboard