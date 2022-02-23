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

#include <d2d1.h>
#include <d3d11.h>
#include <shims/winrt.h>

#include <memory>

#include <OpenKneeboard/Events.h>

namespace OpenKneeboard {
struct DXResources;
class KneeboardState;
class Tab;

class InterprocessRenderer final : private EventReceiver {
 private:
  class Impl;
  std::unique_ptr<Impl> p;

 public:
  InterprocessRenderer() = delete;
  InterprocessRenderer(
    const DXResources&,
    const std::shared_ptr<KneeboardState>&,
    HWND feederWindow);
  ~InterprocessRenderer();

 private:
  void RenderNow();
  void Render(const std::shared_ptr<Tab>& tab, uint16_t pageIndex);
};

}// namespace OpenKneeboard
