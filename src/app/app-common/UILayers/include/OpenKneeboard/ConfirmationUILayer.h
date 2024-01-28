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
#include <OpenKneeboard/IUILayer.h>

namespace OpenKneeboard {

struct CursorEvent;
class IToolbarItemWithConfirmation;

class ConfirmationUILayer final
  : public IUILayer,
    public EventReceiver,
    public std::enable_shared_from_this<ConfirmationUILayer> {
 public:
  static std::shared_ptr<ConfirmationUILayer> Create(
    const std::shared_ptr<DXResources>& dxr,
    const std::shared_ptr<IToolbarItemWithConfirmation>&);
  virtual ~ConfirmationUILayer();

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    const EventContext&,
    const CursorEvent&) override;

  virtual void Render(
    RenderTarget*,
    const NextList&,
    const Context&,
    const D2D1_RECT_F&) override;

  virtual Metrics GetMetrics(const NextList&, const Context&) const override;

  Event<> evClosedEvent;

  ConfirmationUILayer() = delete;

 private:
  ConfirmationUILayer(
    const std::shared_ptr<DXResources>& dxr,
    const std::shared_ptr<IToolbarItemWithConfirmation>&);

  std::shared_ptr<DXResources> mDXResources;
  std::shared_ptr<IToolbarItemWithConfirmation> mItem;
  std::optional<D2D1_RECT_F> mCanvasRect;

  winrt::com_ptr<ID2D1SolidColorBrush> mOverpaintBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mDialogBGBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mTextBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mButtonBorderBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mHoverButtonFillBrush;

  void UpdateLayout(const D2D1_RECT_F&);

  struct TextRenderInfo {
    winrt::hstring mWinString;
    D2D1_SIZE_F mSize {};
  };
  TextRenderInfo GetTextRenderInfo(
    const winrt::com_ptr<IDWriteTextFormat>&,
    FLOAT maxWidth,
    std::string_view utf8) const;

  enum class ButtonAction {
    Confirm,
    Cancel,
  };

  struct Button {
    ButtonAction mAction;
    D2D1_RECT_F mRect {};
    winrt::hstring mLabel;

    inline constexpr auto operator==(const Button& other) const {
      return mAction == other.mAction;
    }
  };

  struct Dialog {
    FLOAT mMargin {};

    D2D1_RECT_F mBoundingBox {};

    winrt::hstring mTitle;
    winrt::com_ptr<IDWriteTextFormat> mTitleFormat;
    D2D1_RECT_F mTitleRect {};

    winrt::hstring mDetails;
    winrt::com_ptr<IDWriteTextFormat> mDetailsFormat;
    D2D1_RECT_F mDetailsRect {};

    std::shared_ptr<CursorClickableRegions<Button>> mButtons;
    winrt::com_ptr<IDWriteTextFormat> mButtonsFormat;
  };
  std::optional<Dialog> mDialog;
};

}// namespace OpenKneeboard
