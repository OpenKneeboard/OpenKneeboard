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
#include <OpenKneeboard/ConfirmationUILayer.h>
#include <OpenKneeboard/IToolbarItemWithConfirmation.h>
#include <OpenKneeboard/ToolbarAction.h>

#include <OpenKneeboard/config.h>

static bool operator==(const D2D1_RECT_F& a, const D2D1_RECT_F& b) noexcept {
  return memcmp(&a, &b, sizeof(D2D1_RECT_F)) == 0;
}

namespace OpenKneeboard {

std::shared_ptr<ConfirmationUILayer> ConfirmationUILayer::Create(
  const DXResources& dxr,
  const std::shared_ptr<IToolbarItemWithConfirmation>& item) {
  return std::shared_ptr<ConfirmationUILayer>(
    new ConfirmationUILayer(dxr, item));
}

ConfirmationUILayer::ConfirmationUILayer(
  const DXResources& dxr,
  const std::shared_ptr<IToolbarItemWithConfirmation>& item)
  : mDXResources(dxr), mItem(item) {
  auto d2d = dxr.mD2DDeviceContext.get();

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

ConfirmationUILayer::~ConfirmationUILayer() {
  RemoveAllEventListeners();
}

void ConfirmationUILayer::PostCursorEvent(
  const NextList& next,
  const Context& context,
  const EventContext& eventContext,
  const CursorEvent& cursorEvent) {
  D2D1_RECT_F rect;
  Dialog dialog;
  try {
    rect = mCanvasRect.value();
    dialog = mDialog.value();
  } catch (const std::bad_optional_access&) {
    next.front()->PostCursorEvent(
      next.subspan(1), context, eventContext, cursorEvent);
    return;
  }

  CursorEvent canvasEvent {cursorEvent};
  canvasEvent.mX *= (rect.right - rect.left);
  canvasEvent.mY *= (rect.bottom - rect.top);

  dialog.mButtons->PostCursorEvent(eventContext, canvasEvent);
}

void ConfirmationUILayer::Render(
  RenderTarget* rt,
  const NextList& next,
  const Context& context,
  const D2D1_RECT_F& rect) {
  next.front()->Render(rt, next.subspan(1), context, rect);

  if (rect != mCanvasRect) {
    UpdateLayout(rect);
  }

  if (!mCanvasRect) {
    return;
  }

  Dialog dialog {};
  try {
    dialog = mDialog.value();
  } catch (const std::bad_optional_access&) {
    return;
  }

  auto d2d = rt->d2d();
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
    const auto rr
      = D2D1::RoundedRect(button.mRect, dialog.mMargin, dialog.mMargin);
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

void ConfirmationUILayer::UpdateLayout(const D2D1_RECT_F& canvasRect) {
  const D2D1_SIZE_F canvasSize {
    canvasRect.right - canvasRect.left,
    canvasRect.bottom - canvasRect.top,
  };

  const auto titleFontSize
    = canvasSize.height * (HeaderPercent / 100.0f) * 0.5f;
  const auto maxTextWidth
    = std::min(titleFontSize * 40, canvasSize.width * 0.8f);

  auto dwf = mDXResources.mDWriteFactory;

  const auto textFontSize = titleFontSize * 0.6f;
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

  auto titleTextInfo = GetTextRenderInfo(
    titleFormat, maxTextWidth, mItem->GetConfirmationTitle());
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
    canvasRect.left + ((canvasSize.width - dialogSize.width) / 2),
    canvasRect.top + ((canvasSize.height - dialogSize.height) / 2),
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

  const auto buttonsWidth
    = confirmButtonSize.width + cancelButtonSize.width + margin;

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

  auto buttons = CursorClickableRegions<Button>::Create(std::vector<Button> {
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
    buttons->evClicked, [weak = weak_from_this()](auto, auto button) {
      auto self = weak.lock();
      if (!self) {
        return;
      }
      if (button.mAction == ButtonAction::Confirm) {
        auto action = std::dynamic_pointer_cast<ToolbarAction>(self->mItem);
        if (action) {
          action->Execute();
        }
      }
      self->evClosedEvent.Emit();
    });

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

  auto dwf = mDXResources.mDWriteFactory;
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
