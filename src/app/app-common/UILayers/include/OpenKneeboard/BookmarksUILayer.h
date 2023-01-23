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

#include <OpenKneeboard/Bookmark.h>
#include <OpenKneeboard/CursorClickableRegions.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/UILayerBase.h>
#include <shims/winrt/base.h>

#include <memory>

namespace OpenKneeboard {

class KneeboardState;
class IKneeboardView;

class BookmarksUILayer final : public UILayerBase, private EventReceiver {
 public:
  BookmarksUILayer(const DXResources& dxr, KneeboardState*, IKneeboardView*);
  virtual ~BookmarksUILayer();

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    const EventContext&,
    const CursorEvent&) override;
  virtual Metrics GetMetrics(const NextList&, const Context&) const override;
  virtual void Render(
    const NextList&,
    const Context&,
    ID2D1DeviceContext*,
    const D2D1_RECT_F&) override;

  BookmarksUILayer() = delete;

 private:
  DXResources mDXResources;
  KneeboardState* mKneeboardState {nullptr};
  IKneeboardView* mKneeboardView;

  winrt::com_ptr<ID2D1SolidColorBrush> mBackgroundBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mTextBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mHoverBrush;

  struct Button {
    D2D1_RECT_F mRect {};
    Bookmark mBookmark {};

    bool operator==(const Button&) const noexcept;
  };

  using Buttons = std::shared_ptr<CursorClickableRegions<Button>>;

  Buttons mButtons;
  Buttons LayoutButtons();

  bool IsEnabled() const;
};

}// namespace OpenKneeboard
