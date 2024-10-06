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

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/Pixels.hpp>
#include <OpenKneeboard/PreferredSize.hpp>
#include <OpenKneeboard/ProcessShutdownBlock.hpp>
#include <OpenKneeboard/RenderTarget.hpp>
#include <OpenKneeboard/ThreadGuard.hpp>
#include <OpenKneeboard/UniqueID.hpp>

#include <OpenKneeboard/inttypes.hpp>

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

  virtual std::optional<PreferredSize> GetPreferredSize(PageID) = 0;
  virtual task<void> RenderPage(RenderContext, PageID, PixelRect rect) = 0;

  Event<> evNeedsRepaintEvent;
  Event<SuggestedPageAppendAction> evPageAppendedEvent;
  Event<> evContentChangedEvent;
  Event<KneeboardViewID, PageID> evPageChangeRequestedEvent;
  Event<> evAvailableFeaturesChangedEvent;

 protected:
  ThreadGuard mThreadGuard;
};

}// namespace OpenKneeboard
