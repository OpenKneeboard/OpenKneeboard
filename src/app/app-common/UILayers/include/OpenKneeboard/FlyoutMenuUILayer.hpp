// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/CursorClickableRegions.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/IToolbarItem.hpp>
#include <OpenKneeboard/IUILayer.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

#include <shims/winrt/base.h>

#include <memory>

namespace OpenKneeboard {

class KneeboardState;

class FlyoutMenuUILayer final
  : public IUILayer,
    public EventReceiver,
    public std::enable_shared_from_this<FlyoutMenuUILayer> {
 public:
  enum class Corner {
    TopLeft,
    TopRight,
  };
  static std::shared_ptr<FlyoutMenuUILayer> Create(
    const audited_ptr<DXResources>& dxr,
    const std::vector<std::shared_ptr<IToolbarItem>>& items,
    D2D1_POINT_2F preferredTopLeft01,
    D2D1_POINT_2F preferredTopRight01,
    Corner preferredCorner);
  virtual ~FlyoutMenuUILayer();

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    KneeboardViewID,
    const CursorEvent&) override;
  virtual Metrics GetMetrics(const NextList&, const Context&) const override;
  [[nodiscard]] task<void> Render(
    const RenderContext&,
    const NextList&,
    const Context&,
    const PixelRect&) override;

  Event<> evCloseMenuRequestedEvent;

  FlyoutMenuUILayer() = delete;

 private:
  FlyoutMenuUILayer(
    const audited_ptr<DXResources>& dxr,
    const std::vector<std::shared_ptr<IToolbarItem>>& items,
    D2D1_POINT_2F preferredTopLeft01,
    D2D1_POINT_2F preferredTopRight01,
    Corner preferredCorner);
  audited_ptr<DXResources> mDXResources;
  std::vector<std::shared_ptr<IToolbarItem>> mItems;
  D2D1_POINT_2F mPreferredTopLeft01 {};
  D2D1_POINT_2F mPreferredTopRight01 {};
  Corner mPreferredAnchor;

  winrt::com_ptr<ID2D1SolidColorBrush> mBGOverpaintBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mMenuBGBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mMenuHoverBGBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mMenuFGBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mMenuDisabledFGBrush;

  std::optional<PixelRect> mLastRenderRect;

  std::shared_ptr<IUILayer> mPrevious;

  struct MenuItem {
    D2D1_RECT_F mRect {};
    std::shared_ptr<IToolbarItem> mItem;

    winrt::hstring mLabel;
    D2D1_RECT_F mLabelRect {};
    winrt::hstring mGlyph;
    D2D1_RECT_F mGlyphRect {};
    D2D1_RECT_F mChevronRect {};

    bool operator==(const MenuItem&) const noexcept;
  };

  struct Menu {
    float mMargin {};
    D2D1_RECT_F mRect {};
    std::shared_ptr<CursorClickableRegions<MenuItem>> mCursorImpl;
    std::vector<D2D1_RECT_F> mSeparatorRects;
    winrt::com_ptr<IDWriteTextFormat> mTextFormat;
    winrt::com_ptr<IDWriteTextFormat> mGlyphFormat;
  };
  std::optional<Menu> mMenu;

  void UpdateLayout(ID2D1DeviceContext*, const PixelRect&);
  void OnClick(const MenuItem&);

  bool mRecursiveCall = false;
};

}// namespace OpenKneeboard
