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
#include <OpenKneeboard/IUILayer.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

namespace OpenKneeboard {

struct CursorEvent;
class IToolbarItemWithConfirmation;

class ConfirmationUILayer final
  : public IUILayer,
    public EventReceiver,
    public std::enable_shared_from_this<ConfirmationUILayer> {
 public:
  static std::shared_ptr<ConfirmationUILayer> Create(
    const audited_ptr<DXResources>& dxr,
    const std::shared_ptr<IToolbarItemWithConfirmation>&);
  virtual ~ConfirmationUILayer();

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    KneeboardViewID,
    const CursorEvent&) override;

  [[nodiscard]] task<void> Render(
    const RenderContext&,
    const NextList&,
    const Context&,
    const PixelRect&) override;

  virtual Metrics GetMetrics(const NextList&, const Context&) const override;

  Event<> evClosedEvent;

  ConfirmationUILayer() = delete;

 private:
  ConfirmationUILayer(
    const audited_ptr<DXResources>& dxr,
    const std::shared_ptr<IToolbarItemWithConfirmation>&);

  audited_ptr<DXResources> mDXResources;
  std::shared_ptr<IToolbarItemWithConfirmation> mItem;
  // TODO: key by RTID
  std::optional<PixelRect> mCanvasRect;

  winrt::com_ptr<ID2D1SolidColorBrush> mOverpaintBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mDialogBGBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mTextBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mButtonBorderBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mHoverButtonFillBrush;

  void UpdateLayout(const PixelRect&);

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
    float mMargin {};

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
