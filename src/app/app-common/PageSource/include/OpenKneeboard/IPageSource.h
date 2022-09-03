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
#include <d2d1_1.h>

#include <cstdint>

namespace OpenKneeboard {

enum class ContentChangeType {
  /// Treat like a new tab
  FullyReplaced,
  /// Content changed, but likely similar; preserve view state like
  /// 'current page' if possible.
  Modified,
};

class IPageSource {
 public:
  virtual ~IPageSource();

  virtual uint16_t GetPageCount() const = 0;
  virtual D2D1_SIZE_U GetNativeContentSize(uint16_t pageIndex) = 0;
  virtual void
  RenderPage(ID2D1DeviceContext*, uint16_t pageIndex, const D2D1_RECT_F& rect)
    = 0;

  Event<> evNeedsRepaintEvent;
  Event<> evPageAppendedEvent;
  Event<ContentChangeType> evContentChangedEvent;
  Event<EventContext, uint16_t> evPageChangeRequestedEvent;
  Event<> evAvailableFeaturesChangedEvent;
};

}// namespace OpenKneeboard
