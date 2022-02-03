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

#include <Windows.h>

#include <cstdint>
#include <memory>

namespace OpenKneeboard {

class WintabTablet final {
 public:
  struct Limits {
    uint32_t x, y, pressure;
  };
  struct State {
    bool active = false;
    uint32_t x = 0, y = 0, pressure = 0;
    uint16_t penButtons = 0, tabletButtons = 0;
  };

  WintabTablet(HWND window);
  ~WintabTablet();

  operator bool() const;

  bool CanProcessMessage(UINT message) const;
  bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

  State GetState() const;
  Limits GetLimits() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

};// namespace OpenKneeboard
