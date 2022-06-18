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
#include <OpenKneeboard/Tab.h>
#include <d2d1.h>

#include <memory>
#include <vector>

namespace OpenKneeboard {

enum class UserAction;
class TabView;
struct CursorEvent;

class KneeboardView final : private EventReceiver {
 public:
  KneeboardView();
  ~KneeboardView();

  // TODO: rename to GetCurrentTabView, then re-add GetCurrentTab
  std::shared_ptr<TabView> GetCurrentTab() const;
  uint8_t GetTabIndex() const;
  std::shared_ptr<TabView> GetTabViewByID(Tab::RuntimeID) const;
  void SetTabByIndex(uint8_t);
  void SetTabByID(Tab::RuntimeID);
  void PreviousTab();
  void NextTab();

  void NextPage();
  void PreviousPage();

  const D2D1_SIZE_U& GetCanvasSize() const;
  const D2D1_RECT_F& GetHeaderRenderRect() const;
  const D2D1_RECT_F& GetContentRenderRect() const;
  /// ContentRenderRect may be scaled; this is the 'real' size.
  const D2D1_SIZE_U& GetContentNativeSize() const;

  Event<uint8_t> evCurrentTabChangedEvent;
  Event<> evNeedsRepaintEvent;

  Event<const CursorEvent&> evCursorEvent;
  bool HaveCursor() const;
  D2D1_POINT_2F GetCursorPoint() const;
  D2D1_POINT_2F GetCursorCanvasPoint() const;
  D2D1_POINT_2F GetCursorCanvasPoint(const D2D1_POINT_2F&) const;

  void SetTabs(const std::vector<std::shared_ptr<Tab>>& tabs);

  void PostCursorEvent(const CursorEvent& ev);
  void PostUserAction(UserAction);

 private:
  std::vector<std::shared_ptr<TabView>> mTabs;
  std::shared_ptr<TabView> mCurrentTab;

  D2D1_SIZE_U mCanvasSize;
  D2D1_SIZE_U mContentNativeSize;
  D2D1_RECT_F mHeaderRenderRect;
  D2D1_RECT_F mContentRenderRect;

  bool mHaveCursor = false;
  D2D1_POINT_2F mCursorPoint;

  void UpdateLayout();
};

}// namespace OpenKneeboard
