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

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/IHasDisposeAsync.hpp>
#include <OpenKneeboard/IPageSource.hpp>
#include <OpenKneeboard/IPageSourceWithCursorEvents.hpp>
#include <OpenKneeboard/IPageSourceWithNavigation.hpp>
#include <OpenKneeboard/KneeboardState.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace OpenKneeboard {

struct DXResources;
class CachedLayer;
class DoodleRenderer;

class PageSourceWithDelegates : public virtual IPageSource,
                                public virtual IPageSourceWithCursorEvents,
                                public virtual IPageSourceWithNavigation,
                                public IHasDisposeAsync,
                                public virtual EventReceiver {
 public:
  PageSourceWithDelegates() = delete;
  PageSourceWithDelegates(const audited_ptr<DXResources>&, KneeboardState*);
  virtual ~PageSourceWithDelegates();

  [[nodiscard]]
  virtual IAsyncAction DisposeAsync() noexcept override;

  virtual PageIndex GetPageCount() const override;
  virtual std::vector<PageID> GetPageIDs() const override;
  virtual PreferredSize GetPreferredSize(PageID) override;
  virtual void RenderPage(const RenderContext&, PageID, const PixelRect& rect)
    override;

  virtual void PostCursorEvent(KneeboardViewID, const CursorEvent&, PageID)
    override;
  virtual bool CanClearUserInput(PageID) const override;
  virtual bool CanClearUserInput() const override;
  virtual void ClearUserInput(PageID) override;
  virtual void ClearUserInput() override;

  virtual bool IsNavigationAvailable() const override;
  virtual std::vector<NavigationEntry> GetNavigationEntries() const override;

 protected:
  DisposalState mDisposal;
  // This is optional, but can only be called when there are no delegates; it
  // will fatal otherwise, as it is unable to clear up previous delegates
  void SetDelegatesFromEmpty(const std::vector<std::shared_ptr<IPageSource>>&);

  [[nodiscard]]
  winrt::Windows::Foundation::IAsyncAction SetDelegates(
    std::vector<std::shared_ptr<IPageSource>>);

 private:
  audited_ptr<DXResources> mDXResources;
  std::vector<std::shared_ptr<IPageSource>> mDelegates;
  std::vector<EventHandlerToken> mDelegateEvents;
  std::vector<EventHandlerToken> mFixedEvents;

  std::shared_ptr<IPageSource> FindDelegate(PageID) const;
  mutable std::unordered_map<PageID, std::weak_ptr<IPageSource>> mPageDelegates;

  std::unordered_map<RenderTargetID, std::unique_ptr<CachedLayer>>
    mContentLayerCache;
  std::unique_ptr<DoodleRenderer> mDoodles;

  void RenderPageWithCache(
    IPageSource* delegate,
    RenderTarget*,
    PageID,
    const PixelRect& rect);
};

}// namespace OpenKneeboard
