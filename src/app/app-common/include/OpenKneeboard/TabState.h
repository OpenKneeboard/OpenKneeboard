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

#include <concepts>
#include <memory>
#include <vector>

#include <OpenKneeboard/Events.h>

namespace OpenKneeboard {

struct CursorEvent;
class Tab;

enum class TabMode {
  NORMAL,
  NAVIGATION,
};

class TabState final : private EventReceiver {
 public:
  TabState(const std::shared_ptr<Tab>&);
  ~TabState();
  uint64_t GetInstanceID() const;

  template <std::derived_from<Tab> T, class... Args>
  static std::shared_ptr<TabState> make_shared(Args... args) {
    auto tab = std::make_shared<T>(args...);
    if (!tab) {
      return {};
    }
    return std::make_shared<TabState>(tab);
  }

  std::shared_ptr<Tab> GetRootTab() const;

  void SetPageIndex(uint16_t);
  void NextPage();
  void PreviousPage();

  std::shared_ptr<Tab> GetTab() const;
  uint16_t GetPageCount() const;
  uint16_t GetPageIndex() const;

  D2D1_SIZE_U GetNativeContentSize() const;

  void PostCursorEvent(const CursorEvent&);

  Event<CursorEvent> evCursorEvent;
  Event<> evNeedsRepaintEvent;
  Event<> evPageChangedEvent;
  Event<uint16_t> evPageChangeRequestedEvent;

  TabMode GetTabMode() const;
  bool SupportsTabMode(TabMode) const;
  bool SetTabMode(TabMode);
 private:
  static uint64_t sNextID;
  uint64_t mInstanceID;
  std::shared_ptr<Tab> mRootTab;
  uint16_t mRootTabPage;

  // For now, just navigation views, maybe more later
  std::shared_ptr<Tab> mActiveSubTab;
  uint16_t mActiveSubTabPage;

  TabMode mTabMode = TabMode::NORMAL;

  void OnTabFullyReplaced();
  void OnTabPageAppended();
};

}// namespace OpenKneeboard
