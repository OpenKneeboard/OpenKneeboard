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
  struct State {
    bool active = false;
    uint32_t x, y;
  };

  WintabTablet(HWND window);
  ~WintabTablet();

  operator bool() const;

  bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);
  State GetState();

 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

};// namespace OpenKneeboard
