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

#include <OpenKneeboard/TabletInfo.hpp>
#include <OpenKneeboard/TabletState.hpp>

#include <Windows.h>

#include <cstdint>
#include <memory>
#include <string>

namespace OpenKneeboard {

class WintabTablet final {
 public:
  enum class Priority {
    AlwaysActive,
    ForegroundOnly,
  };

  WintabTablet() = delete;
  WintabTablet(HWND window, Priority priority);
  ~WintabTablet();

  bool IsValid() const;
  Priority GetPriority() const;
  void SetPriority(Priority);

  bool CanProcessMessage(UINT message) const;
  bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

  TabletState GetState() const;
  TabletInfo GetDeviceInfo() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

}// namespace OpenKneeboard
