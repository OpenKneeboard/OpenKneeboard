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

#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/IPageSource.h>
#include <OpenKneeboard/ITabView.h>
#include <OpenKneeboard/ThreadGuard.h>

#include <vector>

namespace OpenKneeboard {

class KneeboardState;

class TabView final : public ITabView, private EventReceiver {
 public:
  TabView(const DXResources&, KneeboardState*, const std::shared_ptr<ITab>&);
  ~TabView();

  virtual void SetPageID(PageID) override;
  virtual PageID GetPageID() const override;
  virtual std::vector<PageID> GetPageIDs() const override;

  virtual std::shared_ptr<ITab> GetRootTab() const override;

  virtual std::shared_ptr<ITab> GetTab() const override;

  virtual D2D1_SIZE_U GetNativeContentSize() const override;

  virtual void PostCursorEvent(const CursorEvent&) override;

  virtual TabMode GetTabMode() const override;
  virtual bool SupportsTabMode(TabMode) const override;
  virtual bool SetTabMode(TabMode) override;

 private:
  const EventContext mEventContext;

  DXResources mDXR;
  KneeboardState* mKneeboard;

  std::shared_ptr<ITab> mRootTab;
  struct PagePosition {
    PageID mID;
    // The ID is the source of truth (so e.g. bookmarks and doodles stay on the
    // correct page after pages are added/removed), but use the index to detect
    // page prepends.
    //
    // For now, this is used to stay on the 'first page' if pages are prepended
    // (especially while loading a folder tab), though I'm not certain what will
    // feel best.
    PageIndex mIndex;
  };
  std::optional<PagePosition> mRootTabPage;

  // For now, just navigation views, maybe more later
  std::shared_ptr<ITab> mActiveSubTab;
  std::optional<PageID> mActiveSubTabPageID;

  TabMode mTabMode = TabMode::NORMAL;

  void OnTabContentChanged();
  void OnTabPageAppended(SuggestedPageAppendAction);

  ThreadGuard mThreadGuard;
};

}// namespace OpenKneeboard
