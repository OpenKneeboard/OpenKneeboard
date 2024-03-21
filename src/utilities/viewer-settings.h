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

#include <OpenKneeboard/json_fwd.h>

#include <shims/winrt/base.h>

namespace OpenKneeboard {

enum class ViewerFillMode {
  Default,
  Checkerboard,
  Transparent,
};

enum class ViewerAlignment {
  TopLeft,
  Top,
  TopRight,
  Left,
  Center,
  Right,
  BottomLeft,
  Bottom,
  BottomRight,
};

struct ViewerSettings final {
  int mWindowWidth {768 / 2};
  int mWindowHeight {1024 / 2};
  int mWindowX {CW_USEDEFAULT};
  int mWindowY {CW_USEDEFAULT};

  bool mStreamerMode {false};
  ViewerFillMode mFillMode {ViewerFillMode::Default};
  ViewerAlignment mAlignment {ViewerAlignment::Center};

  static ViewerSettings Load();
  void Save();

  constexpr auto operator<=>(const ViewerSettings&) const noexcept = default;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(ViewerSettings)

}// namespace OpenKneeboard
