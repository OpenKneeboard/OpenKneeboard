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

#include <d2d1_1.h>
#include <shims/winrt.h>

#include <memory>

namespace OpenKneeboard {

class D2DErrorRenderer final {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  D2DErrorRenderer(const winrt::com_ptr<ID2D1DeviceContext>& ctx);
  D2DErrorRenderer() = delete;
  ~D2DErrorRenderer();

  void Render(const std::wstring& text, const D2D1_RECT_F& where);
};

}// namespace OpenKneeboard
