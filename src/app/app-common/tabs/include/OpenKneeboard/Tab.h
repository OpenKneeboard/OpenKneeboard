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

#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/utf8.h>
#include <d2d1_1.h>

#include <string>

namespace OpenKneeboard {

class Tab {
 public:
  virtual utf8_string GetGlyph() const = 0;
  virtual utf8_string GetTitle() const = 0;
  virtual void Reload() = 0;

  virtual uint16_t GetPageCount() const = 0;
  virtual D2D1_SIZE_U GetNativeContentSize(uint16_t pageIndex) = 0;
  virtual void
  RenderPage(ID2D1DeviceContext*, uint16_t pageIndex, const D2D1_RECT_F& rect)
    = 0;

  Event<> evNeedsRepaintEvent;
  Event<> evFullyReplacedEvent;
  Event<> evAvailableFeaturesChangedEvent;
  Event<> evPageAppendedEvent;
  Event<EventContext, uint16_t> evPageChangeRequestedEvent;
};
}// namespace OpenKneeboard
