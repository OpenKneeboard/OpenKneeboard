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

#include <OpenKneeboard/utf8.h>

#include <shims/winrt/base.h>

#include <memory>

#include <d2d1_1.h>

namespace OpenKneeboard {

struct DXResources;

class D2DErrorRenderer final {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  D2DErrorRenderer(const std::shared_ptr<DXResources>&);
  D2DErrorRenderer(IDWriteFactory*, ID2D1SolidColorBrush*);
  D2DErrorRenderer() = delete;
  ~D2DErrorRenderer();

  void Render(
    ID2D1DeviceContext*,
    std::string_view text,
    const D2D1_RECT_F& where,
    ID2D1Brush* brush = nullptr);
};

}// namespace OpenKneeboard
