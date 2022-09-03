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
#include <OpenKneeboard/IPageSource.h>
#include <OpenKneeboard/IPageSourceWithCursorEvents.h>
#include <OpenKneeboard/IPageSourceWithNavigation.h>

#include <memory>
#include <tuple>
#include <vector>

namespace OpenKneeboard {

struct DXResources;
class CachedLayer;
class DoodleRenderer;
class KneeboardState;

class PageSourceWithDelegates : public virtual IPageSource,
                                public virtual IPageSourceWithCursorEvents,
                                public virtual IPageSourceWithNavigation,
                                protected EventReceiver {
 public:
  PageSourceWithDelegates() = delete;
  PageSourceWithDelegates(const DXResources&, KneeboardState*);
  virtual ~PageSourceWithDelegates();

  virtual uint16_t GetPageCount() const override;
  virtual D2D1_SIZE_U GetNativeContentSize(uint16_t pageIndex) override;
  virtual void RenderPage(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F& rect) override;

  virtual void PostCursorEvent(
    EventContext,
    const CursorEvent&,
    uint16_t pageIndex) override;

  virtual bool IsNavigationAvailable() const override;
  virtual std::vector<NavigationEntry> GetNavigationEntries() const override;

 protected:
  void SetDelegates(const std::vector<std::shared_ptr<IPageSource>>&);

 private:
  std::vector<std::shared_ptr<IPageSource>> mDelegates;
  std::vector<EventHandlerToken> mDelegateEvents;
  std::vector<EventHandlerToken> mFixedEvents;

  std::tuple<std::shared_ptr<IPageSource>, uint16_t> DecodePageIndex(
    uint16_t) const;

  std::unique_ptr<CachedLayer> mContentLayerCache;
  std::unique_ptr<DoodleRenderer> mDoodles;
};

}// namespace OpenKneeboard
