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
#include <OpenKneeboard/IPageSource.hpp>
#include <OpenKneeboard/KneeboardViewID.hpp>
#include <OpenKneeboard/ThreadGuard.hpp>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/inttypes.hpp>

#include <memory>
#include <vector>

#include <d2d1.h>

namespace OpenKneeboard {

struct CursorEvent;
class ITab;
class KneeboardState;

enum class TabMode {
  Normal,
  Navigation,
};

class TabView final : private EventReceiver {
 public:
  TabView() = delete;
  TabView(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const std::shared_ptr<ITab>&,
    KneeboardViewID);
  ~TabView();

  class RuntimeID final : public UniqueIDBase<RuntimeID> {};
  constexpr auto GetRuntimeID() const noexcept {
    return mRuntimeID;
  }

  void SetPageID(PageID);
  PageID GetPageID() const;
  std::vector<PageID> GetPageIDs() const;

  std::weak_ptr<ITab> GetRootTab() const;

  std::weak_ptr<ITab> GetTab() const;

  std::optional<PreferredSize> GetPreferredSize() const;

  void PostCursorEvent(const CursorEvent&);

  TabMode GetTabMode() const;
  bool SupportsTabMode(TabMode) const;
  bool SetTabMode(TabMode);

  Event<CursorEvent> evCursorEvent;
  Event<> evNeedsRepaintEvent;
  Event<> evPageChangedEvent;
  Event<> evContentChangedEvent;
  Event<PageIndex> evPageChangeRequestedEvent;
  Event<> evAvailableFeaturesChangedEvent;
  Event<> evTabModeChangedEvent;
  Event<> evBookmarksChangedEvent;

 private:
  const RuntimeID mRuntimeID;

  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard;
  std::weak_ptr<ITab> mRootTab;

  // Each TabView should only be used by a single kneeboard view; keep track so
  // we know which events affect us
  const KneeboardViewID mKneeboardViewID {nullptr};

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

  TabMode mTabMode = TabMode::Normal;

  void OnTabContentChanged();
  void OnTabPageAppended(SuggestedPageAppendAction);

  ThreadGuard mThreadGuard;
};

}// namespace OpenKneeboard
