// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/ConfirmationUILayer.hpp>
#include <OpenKneeboard/IToolbarItemWithConfirmation.hpp>
#include <OpenKneeboard/ToolbarAction.hpp>

#include <OpenKneeboard/config.hpp>

namespace OpenKneeboard {

std::shared_ptr<ConfirmationUILayer> ConfirmationUILayer::Create(
  const audited_ptr<DXResources>& dxr,
  const std::shared_ptr<IToolbarItemWithConfirmation>& item) {
  return std::shared_ptr<ConfirmationUILayer>(
    new ConfirmationUILayer(dxr, item));
}

ConfirmationUILayer::ConfirmationUILayer(
  const audited_ptr<DXResources>& dxr,
  const std::shared_ptr<IToolbarItemWithConfirmation>& item)
  : mDXResources(dxr),
    mItem(item) {
  auto d2d = dxr->mD2DDeviceContext.get();

  d2d->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 0.6f}, D2D1::BrushProperties(), mOverpaintBrush.put());
  d2d->CreateSolidColorBrush(
    {1.0f, 1.0f, 1.0f, 1.0f}, D2D1::BrushProperties(), mDialogBGBrush.put());
  d2d->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f}, D2D1::BrushProperties(), mTextBrush.put());
  d2d->CreateSolidColorBrush(
    {0.3f, 0.3f, 0.3f, 1.0f},
    D2D1::BrushProperties(),
    mButtonBorderBrush.put());
  d2d->CreateSolidColorBrush(
    {0.0f, 0.8f, 1.0f, 1.0f},
    D2D1::BrushProperties(),
    mHoverButtonFillBrush.put());
}

ConfirmationUILayer::~ConfirmationUILayer() { RemoveAllEventListeners(); }

void ConfirmationUILayer::PostCursorEvent(
  const NextList& next,
  const Context& context,
  KneeboardViewID KneeboardViewID,
  const CursorEvent& cursorEvent) {
  if (!(mCanvasRect && mDialog)) {
    next.front()->PostCursorEvent(
      next.subspan(1), context, KneeboardViewID, cursorEvent);
    return;
  }

  const auto rect = mCanvasRect.value();
  const auto dialog = mDialog.value();

  CursorEvent canvasEvent {cursorEvent};
  canvasEvent.mX *= rect.Width();
  canvasEvent.mY *= rect.Height();
  canvasEvent.mX += rect.Left();
  canvasEvent.mY += rect.Top();

  dialog.mButtons->PostCursorEvent(KneeboardViewID, canvasEvent);
}

task<void> ConfirmationUILayer::Render(
  const RenderContext& rc,
  const NextList& next,
  const Context& context,
  const PixelRect& rect) {
  OPENKNEEBOARD_TraceLoggingScope("ConfirmationUILayer::Render()");
  co_await next.front()->Render(rc, next.subspan(1), context, rect);

  if (rect != mCanvasRect) {
    UpdateLayout(rect);
  }

  if (!mCanvasRect) {
    co_return;
  }

  Dialog dialog {};
  try {
    dialog = mDialog.value();
  } catch (const std::bad_optional_access&) {
    co_return;
  }

  auto d2d = rc.d2d();
  d2d->FillRectangle(rect, mOverpaintBrush.get());
  d2d->FillRoundedRectangle(
    D2D1::RoundedRect(dialog.mBoundingBox, dialog.mMargin, dialog.mMargin),
    mDialogBGBrush.get());

  d2d->DrawTextW(
    dialog.mTitle.data(),
    dialog.mTitle.size(),
    dialog.mTitleFormat.get(),
    dialog.mTitleRect,
    mTextBrush.get());

  d2d->DrawTextW(
    dialog.mDetails.data(),
    dialog.mDetails.size(),
    dialog.mDetailsFormat.get(),
    dialog.mDetailsRect,
    mTextBrush.get());

  const auto [hoverButton, buttons] = dialog.mButtons->GetState();
  for (const auto& button: buttons) {
    const auto rr =
      D2D1::RoundedRect(button.mRect, dialog.mMargin, dialog.mMargin);
    if (button == hoverButton) {
      d2d->FillRoundedRectangle(rr, mHoverButtonFillBrush.get());
    }

    d2d->DrawRoundedRectangle(rr, mButtonBorderBrush.get(), 2.0f);
    d2d->DrawTextW(
      button.mLabel.data(),
      button.mLabel.size(),
      dialog.mButtonsFormat.get(),
      button.mRect,
      mTextBrush.get());
  }
}

void ConfirmationUILayer::UpdateLayout(const PixelRect& canvasRect) {
  const auto canvasSize = canvasRect.mSize;

  const auto titleFontSize =
    std::round(canvasSize.mHeight * (HeaderPercent / 100.0f) * 0.5f);
  const auto maxTextWidth =
    std::floor(std::min<float>(titleFontSize * 40, canvasSize.mWidth * 0.8f));

  auto dwf = mDXResources->mDWriteFactory;

  winrt::com_ptr<IDWriteTextFormat> titleFormat;
  winrt::check_hresult(dwf->CreateTextFormat(
    VariableWidthUIFont,
    nullptr,
    DWRITE_FONT_WEIGHT_BOLD,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    titleFontSize,
    L"en-us",
    titleFormat.put()));
  titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  titleFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

  const auto detailsFontSize = titleFontSize * 0.6f;
  winrt::com_ptr<IDWriteTextFormat> detailsFormat;
  winrt::check_hresult(dwf->CreateTextFormat(
    VariableWidthUIFont,
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    detailsFontSize,
    L"en-us",
    detailsFormat.put()));
  detailsFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  detailsFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

  const auto buttonFontSize = titleFontSize * 0.6f;
  winrt::com_ptr<IDWriteTextFormat> buttonFormat;
  winrt::check_hresult(dwf->CreateTextFormat(
    VariableWidthUIFont,
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    buttonFontSize,
    L"en-us",
    buttonFormat.put()));
  buttonFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  buttonFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

  auto titleTextInfo =
    GetTextRenderInfo(titleFormat, maxTextWidth, mItem->GetConfirmationTitle());
  auto detailsTextInfo = GetTextRenderInfo(
    detailsFormat, maxTextWidth, mItem->GetConfirmationDescription());
  auto confirmButtonTextInfo = GetTextRenderInfo(
    buttonFormat, maxTextWidth / 2, mItem->GetConfirmButtonLabel());
  auto cancelButtonTextInfo = GetTextRenderInfo(
    buttonFormat, maxTextWidth / 2, mItem->GetCancelButtonLabel());

  const auto margin = titleFontSize / 3;
  /* Horizontal margins:
   *
   * - left -> content
   * - content -> right
   *
   * - button left -> button text
   * - button right -> button text
   * - space between buttons
   * - button left -> button text
   * - button text -> button right
   *
   * Vertical margins:
   *
   * - top -> title
   * - title -> detail
   * - detail -> buttons
   * - 0.5: button top -> button text
   * - 0.5: button text -> button bottom
   * - button bottom -> bottom
   */
  const D2D1_SIZE_F dialogSize = {
    std::max({
      titleTextInfo.mSize.width,
      detailsTextInfo.mSize.width,
      confirmButtonTextInfo.mSize.width + cancelButtonTextInfo.mSize.width
        + (margin * 5),
    }) + (margin * 2),
    (margin * 5) + titleTextInfo.mSize.height + detailsTextInfo.mSize.height
      + std::max(
        confirmButtonTextInfo.mSize.height, cancelButtonTextInfo.mSize.height),
  };

  const D2D1_POINT_2F dialogOrigin {
    canvasRect.Left() + ((canvasSize.mWidth - dialogSize.width) / 2),
    canvasRect.Top() + ((canvasSize.mHeight - dialogSize.height) / 2),
  };

  const D2D1_RECT_F dialogRect {
    dialogOrigin.x,
    dialogOrigin.y,
    dialogOrigin.x + dialogSize.width,
    dialogOrigin.y + dialogSize.height,
  };

  D2D1_POINT_2F cursor {
    dialogOrigin.x + margin,
    dialogOrigin.y + margin,
  };

  const D2D1_RECT_F titleRect {
    cursor.x,
    cursor.y,
    dialogRect.right - margin,
    cursor.y + titleTextInfo.mSize.height,
  };

  cursor.y = titleRect.bottom + margin;

  const D2D1_RECT_F detailsRect {
    cursor.x,
    cursor.y,
    dialogRect.right - margin,
    cursor.y + detailsTextInfo.mSize.height,
  };

  cursor.y = detailsRect.bottom + margin;

  const D2D1_SIZE_F confirmButtonSize {
    confirmButtonTextInfo.mSize.width + (2 * margin),
    confirmButtonTextInfo.mSize.height + margin,
  };

  const D2D1_SIZE_F cancelButtonSize {
    cancelButtonTextInfo.mSize.width + (2 * margin),
    confirmButtonSize.height,
  };

  const auto buttonsWidth =
    confirmButtonSize.width + cancelButtonSize.width + margin;

  cursor.x = dialogRect.left + ((dialogSize.width - buttonsWidth) / 2);

  const D2D1_RECT_F confirmButtonRect {
    cursor.x,
    cursor.y,
    cursor.x + confirmButtonSize.width,
    cursor.y + confirmButtonSize.height,
  };

  cursor.x = confirmButtonRect.right + margin;

  const D2D1_RECT_F cancelButtonRect {
    cursor.x,
    cursor.y,
    cursor.x + cancelButtonSize.width,
    cursor.y + cancelButtonSize.height,
  };

  auto buttons = CursorClickableRegions<Button>::Create(
    std::vector<Button> {
      Button {
        .mAction = ButtonAction::Confirm,
        .mRect = confirmButtonRect,
        .mLabel = confirmButtonTextInfo.mWinString,
      },
      Button {
        .mAction = ButtonAction::Cancel,
        .mRect = cancelButtonRect,
        .mLabel = cancelButtonTextInfo.mWinString,
      },
    });
  AddEventListener(
    buttons->evClicked,
    [](auto self, auto, auto button) -> OpenKneeboard::fire_and_forget {
      if (button.mAction == ButtonAction::Confirm) {
        auto action = std::dynamic_pointer_cast<ToolbarAction>(self->mItem);
        if (action) {
          co_await action->Execute();
        }
      }
      self->evClosedEvent.Emit();
    } | bind_refs_front(this));

  mDialog = Dialog {
    .mMargin = margin,
    .mBoundingBox = dialogRect,
    .mTitle = titleTextInfo.mWinString,
    .mTitleFormat = std::move(titleFormat),
    .mTitleRect = titleRect,
    .mDetails = detailsTextInfo.mWinString,
    .mDetailsFormat = std::move(detailsFormat),
    .mDetailsRect = detailsRect,
    .mButtons = std::move(buttons),
    .mButtonsFormat = std::move(buttonFormat),
  };
  mCanvasRect = canvasRect;
}

ConfirmationUILayer::TextRenderInfo ConfirmationUILayer::GetTextRenderInfo(
  const winrt::com_ptr<IDWriteTextFormat>& format,
  FLOAT maxWidth,
  std::string_view utf8) const {
  const auto inf = std::numeric_limits<FLOAT>::infinity();

  TextRenderInfo ret {
    .mWinString = winrt::to_hstring(utf8),
  };

  auto dwf = mDXResources->mDWriteFactory;
  winrt::com_ptr<IDWriteTextLayout> layout;
  winrt::check_hresult(dwf->CreateTextLayout(
    ret.mWinString.data(),
    ret.mWinString.size(),
    format.get(),
    maxWidth,
    inf,
    layout.put()));
  DWRITE_TEXT_METRICS metrics {};
  winrt::check_hresult(layout->GetMetrics(&metrics));
  ret.mSize = {metrics.width, metrics.height};
  return ret;
}

IUILayer::Metrics ConfirmationUILayer::GetMetrics(
  const NextList& next,
  const Context& context) const {
  return next.front()->GetMetrics(next.subspan(1), context);
}

}// namespace OpenKneeboard
