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

#include <OpenKneeboard/IKneeboardView.h>

#include <vector>

namespace OpenKneeboard {

enum class UserAction;
class ITabView;
struct CursorEvent;
class KneeboardState;

class KneeboardView final : public IKneeboardView, private EventReceiver {
 public:
  KneeboardView(KneeboardState*);
  ~KneeboardView();

  void SetTabs(const std::vector<std::shared_ptr<Tab>>& tabs);
  void PostUserAction(UserAction);

  virtual std::shared_ptr<ITabView> GetCurrentTabView() const override;
  virtual std::shared_ptr<Tab> GetCurrentTab() const override;
  virtual uint8_t GetTabIndex() const override;
  virtual std::shared_ptr<ITabView> GetTabViewByID(
    Tab::RuntimeID) const override;
  virtual void SetCurrentTabByIndex(uint8_t) override;
  virtual void SetCurrentTabByID(Tab::RuntimeID) override;

  virtual void PreviousTab() override;
  virtual void NextTab() override;

  virtual void NextPage() override;
  virtual void PreviousPage() override;

  virtual const D2D1_SIZE_U& GetCanvasSize() const override;
  virtual const D2D1_RECT_F& GetHeaderRenderRect() const override;
  virtual const D2D1_RECT_F& GetContentRenderRect() const override;
  /// ContentRenderRect may be scaled; this is the 'real' size.
  virtual const D2D1_SIZE_U& GetContentNativeSize() const override;

  virtual bool HaveCursor() const override;
  virtual D2D1_POINT_2F GetCursorPoint() const override;
  virtual D2D1_POINT_2F GetCursorCanvasPoint() const override;
  virtual D2D1_POINT_2F GetCursorCanvasPoint(
    const D2D1_POINT_2F&) const override;

  virtual void PostCursorEvent(const CursorEvent& ev) override;

 private:
  KneeboardState* mKneeboard;
  std::vector<std::shared_ptr<ITabView>> mTabs;
  std::shared_ptr<ITabView> mCurrentTabView;

  D2D1_SIZE_U mCanvasSize;
  D2D1_SIZE_U mContentNativeSize;
  D2D1_RECT_F mHeaderRenderRect;
  D2D1_RECT_F mContentRenderRect;

  bool mHaveCursor = false;
  D2D1_POINT_2F mCursorPoint;

  void UpdateLayout();
};

}// namespace OpenKneeboard
