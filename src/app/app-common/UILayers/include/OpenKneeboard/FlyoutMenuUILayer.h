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
#include <OpenKneeboard/IToolbarItem.h>
#include <OpenKneeboard/IUILayer.h>
#include <shims/winrt/base.h>

#include <memory>

namespace OpenKneeboard {

class KneeboardState;

class FlyoutMenuUILayer final : public IUILayer {
 public:
  enum class Corner {
    TopLeft,
    TopRight,
  };
  FlyoutMenuUILayer(
    const DXResources& dxr,
    const std::vector<std::shared_ptr<IToolbarItem>>& items,
    D2D1_POINT_2F preferredTopLeft01,
    D2D1_POINT_2F preferredTopRight01,
    Corner preferredCorner);
  virtual ~FlyoutMenuUILayer();

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

  FlyoutMenuUILayer() = delete;

 private:
  DXResources mDXResources;
  std::vector<std::shared_ptr<IToolbarItem>> mItems;
  D2D1_POINT_2F mPreferredTopLeft01 {};
  D2D1_POINT_2F mPreferredTopRight01 {};
  Corner mPreferredAnchor;

  winrt::com_ptr<ID2D1SolidColorBrush> mMenuBGBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mMenuFGBrush;

  std::optional<D2D1_RECT_F> mLastRenderRect;

  struct MenuItem {
    D2D1_RECT_F mRect {};
    std::shared_ptr<IToolbarItem> mItem;

    winrt::hstring mLabel;
    D2D1_RECT_F mLabelRect {};

    bool operator==(const MenuItem&) const noexcept;
  };

  struct Menu {
    D2D1_RECT_F mRect {};
    std::unique_ptr<CursorClickableRegions<MenuItem>> mItems;
  };
  std::optional<Menu> mMenu;

  void UpdateLayout(ID2D1DeviceContext*, const D2D1_RECT_F&);
};

}// namespace OpenKneeboard
