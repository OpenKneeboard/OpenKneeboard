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

#include "Events.h"

#include <memory>
#include <vector>

namespace OpenKneeboard {

struct CursorEvent;
struct GameEvent;
class TabState;

class KneeboardState final : private EventReceiver {
 public:
  KneeboardState();
  ~KneeboardState();

  std::vector<std::shared_ptr<TabState>> GetTabs() const;
  void SetTabs(const std::vector<std::shared_ptr<TabState>>& tabs);
  void InsertTab(uint8_t index, const std::shared_ptr<TabState>& tab);
  void AppendTab(const std::shared_ptr<TabState>& tab);
  void RemoveTab(uint8_t index);

  std::shared_ptr<TabState> GetCurrentTab() const;
  uint8_t GetTabIndex() const;
  void SetTabIndex(uint8_t);

  void NextPage();
  void PreviousPage();

  const D2D1_SIZE_U& GetCanvasSize() const;
  const D2D1_RECT_F& GetHeaderRenderRect() const;
  const D2D1_RECT_F& GetContentRenderRect() const;
  /// ContentRenderRect may be scaled; this is the 'real' size.
  const D2D1_SIZE_U& GetContentNativeSize() const;

  bool HaveCursor() const;
  D2D1_POINT_2F GetCursorPoint() const;

  Event<> evFlushEvent;
  Event<> evNeedsRepaintEvent;
  Event<const CursorEvent&> evCursorEvent;

  void PostGameEvent(const GameEvent&);
 private:
  std::vector<std::shared_ptr<TabState>> mTabs;
  std::shared_ptr<TabState> mCurrentTab = nullptr;

  D2D1_SIZE_U mCanvasSize;
  D2D1_SIZE_U mContentNativeSize;
  D2D1_RECT_F mHeaderRenderRect;
  D2D1_RECT_F mContentRenderRect;

  bool mHaveCursor = false;
  D2D1_POINT_2F mCursorPoint;

  void UpdateLayout();

  void OnCursorEvent(const CursorEvent& ev);
};

}// namespace OpenKneeboard
