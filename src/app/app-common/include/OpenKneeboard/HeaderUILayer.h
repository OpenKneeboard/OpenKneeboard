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

#include <OpenKneeboard/CursorClickableRegions.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/ITabView.h>
#include <OpenKneeboard/IUILayer.h>
#include <OpenKneeboard/TabAction.h>
#include <shims/winrt/base.h>

#include <memory>

namespace OpenKneeboard {

class KneeboardState;

class HeaderUILayer final : public IUILayer, private EventReceiver {
 public:
  HeaderUILayer(const DXResources& dxr, KneeboardState*);
  virtual ~HeaderUILayer();

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    const EventContext&,
    const CursorEvent&) override;
  virtual D2D1_SIZE_F GetPreferredSize(const NextList&, const Context&)
    const override;
  virtual void Render(
    const NextList&,
    const Context&,
    ID2D1DeviceContext*,
    const D2D1_RECT_F&) override;

 private:
  void DrawHeaderText(
    const std::shared_ptr<ITabView>&,
    ID2D1DeviceContext*,
    const D2D1_RECT_F& rect,
    const D2D1_SIZE_F& size) const;
  void DrawToolbar(
    const Context&,
    ID2D1DeviceContext*,
    const D2D1_RECT_F& rect,
    const D2D1_SIZE_F& size);
  void LayoutToolbar(
    const Context&,
    const D2D1_RECT_F& rect,
    const D2D1_SIZE_F& size);

  DXResources mDXResources;
  KneeboardState* mKneeboard {nullptr};
  winrt::com_ptr<ID2D1Brush> mHeaderBGBrush;
  winrt::com_ptr<ID2D1Brush> mHeaderTextBrush;
  winrt::com_ptr<ID2D1Brush> mButtonBrush;
  winrt::com_ptr<ID2D1Brush> mDisabledButtonBrush;
  winrt::com_ptr<ID2D1Brush> mHoverButtonBrush;
  winrt::com_ptr<ID2D1Brush> mActiveButtonBrush;
  EventContext mEventContext;

  struct Button {
    D2D1_RECT_F mRect {};
    std::shared_ptr<TabAction> mAction;

    bool operator==(const Button&) const noexcept;
  };

  struct Toolbar {
    std::weak_ptr<ITabView> mTabView {};
    D2D1_RECT_F mRect {};
    std::unique_ptr<CursorClickableRegions<Button>> mButtons;
  };
  std::optional<Toolbar> mToolbar;
};

}// namespace OpenKneeboard
