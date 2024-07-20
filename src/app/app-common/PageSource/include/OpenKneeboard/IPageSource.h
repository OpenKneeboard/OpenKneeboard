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
#include <OpenKneeboard/Pixels.h>
#include <OpenKneeboard/PreferredSize.h>
#include <OpenKneeboard/ProcessShutdownBlock.h>
#include <OpenKneeboard/RenderTarget.h>
#include <OpenKneeboard/ThreadGuard.h>
#include <OpenKneeboard/UniqueID.h>

#include <OpenKneeboard/inttypes.h>

#include <cstdint>

#include <d2d1_1.h>

namespace OpenKneeboard {

enum class SuggestedPageAppendAction {
  SwitchToNewPage,
  KeepOnCurrentPage,
};

class IPageSource {
 private:
  ProcessShutdownBlock mBlockShutdownUntilDestroyed;

 public:
  virtual ~IPageSource();

  virtual PageIndex GetPageCount() const = 0;
  virtual std::vector<PageID> GetPageIDs() const = 0;

  virtual PreferredSize GetPreferredSize(PageID) = 0;
  virtual void RenderPage(const RenderContext&, PageID, const PixelRect& rect)
    = 0;

  Event<> evNeedsRepaintEvent;
  Event<SuggestedPageAppendAction> evPageAppendedEvent;
  Event<> evContentChangedEvent;
  Event<EventContext, PageID> evPageChangeRequestedEvent;
  Event<> evAvailableFeaturesChangedEvent;
  ThreadGuard mThreadGuard;
};

}// namespace OpenKneeboard
