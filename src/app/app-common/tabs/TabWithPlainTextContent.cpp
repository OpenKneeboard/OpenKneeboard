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
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/TabWithPlainTextContent.h>
#include <OpenKneeboard/dprint.h>
#include <Unknwn.h>
#include <dwrite.h>
#include <fmt/format.h>
#include <fmt/xchar.h>

#include <algorithm>

namespace OpenKneeboard {

TabWithPlainTextContent::TabWithPlainTextContent(const DXResources& dxr)
  : mDXR(dxr) {
  auto dwf = mDXR.mDWriteFactory;
  dwf->CreateTextFormat(
    L"Consolas",
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    20.0f * RENDER_SCALE,
    L"",
    mTextFormat.put());

  const auto size = this->GetNativeContentSize(0);
  winrt::com_ptr<IDWriteTextLayout> textLayout;
  dwf->CreateTextLayout(
    L"m", 1, mTextFormat.get(), size.width, size.height, textLayout.put());
  DWRITE_TEXT_METRICS metrics;
  textLayout->GetMetrics(&metrics);

  mPadding = mRowHeight = metrics.height;
  mRows = static_cast<int>((size.height - (2 * mPadding)) / metrics.height) - 2;
  mColumns = static_cast<int>((size.width - (2 * mPadding)) / metrics.width);
}

TabWithPlainTextContent::~TabWithPlainTextContent() {
}

uint16_t TabWithPlainTextContent::GetPageCount() const {
  if (mCompletePages.empty() && mCurrentPageLines.empty()) {
    return 0;
  }

  // We only push a complete page when there's content (or about to be)
  return mCompletePages.size() + 1;
}

D2D1_SIZE_U TabWithPlainTextContent::GetNativeContentSize(uint16_t pageIndex) {
  return {768 * RENDER_SCALE, 1024 * RENDER_SCALE};
}

void TabWithPlainTextContent::RenderPlainTextContent(
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  const auto virtualSize = GetNativeContentSize(0);
  const D2D1_SIZE_F canvasSize {rect.right - rect.left, rect.bottom - rect.top};

  const auto scaleX = canvasSize.width / virtualSize.width;
  const auto scaleY = canvasSize.height / virtualSize.height;
  const auto scale = std::min(scaleX, scaleY);
  const D2D1_SIZE_F renderSize {
    scale * virtualSize.width, scale * virtualSize.height};

  ctx->SetTransform(
    D2D1::Matrix3x2F::Scale(scale, scale)
    * D2D1::Matrix3x2F::Translation(
      rect.left + ((canvasSize.width - renderSize.width) / 2),
      rect.top + ((canvasSize.height - renderSize.height) / 2)));

  winrt::com_ptr<ID2D1SolidColorBrush> background;
  winrt::com_ptr<ID2D1SolidColorBrush> textBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> footerBrush;
  ctx->CreateSolidColorBrush({1.0f, 1.0f, 1.0f, 1.0f}, background.put());
  ctx->CreateSolidColorBrush({0.0f, 0.0f, 0.0f, 1.0f}, textBrush.put());
  ctx->CreateSolidColorBrush({0.5f, 0.5f, 0.5f, 1.0f}, footerBrush.put());

  ctx->FillRectangle(
    {0.0f,
     0.0f,
     static_cast<float>(virtualSize.width),
     static_cast<float>(virtualSize.height)},
    background.get());

  auto textFormat = mTextFormat.get();
  textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  if (mCurrentPageLines.empty()) {
    auto message = winrt::to_hstring(this->GetPlaceholderText());
    ctx->DrawTextW(
      message.data(),
      static_cast<UINT32>(message.size()),
      textFormat,
      {mPadding, mPadding, virtualSize.width - mPadding, mPadding + mRowHeight},
      footerBrush.get());
    return;
  }
  const auto& lines = (pageIndex == mCompletePages.size())
    ? mCurrentPageLines
    : mCompletePages.at(pageIndex);

  D2D_POINT_2F point {mPadding, mPadding};
  for (const auto& line: lines) {
    ctx->DrawTextW(
      line.data(),
      static_cast<UINT32>(line.size()),
      textFormat,
      {point.x, point.y, virtualSize.width - point.x, point.y + mRowHeight},
      textBrush.get());
    point.y += mRowHeight;
  }

  point.y = virtualSize.height - (mRowHeight + mPadding);

  if (pageIndex > 0) {
    std::wstring_view text(L"<<<<<");
    ctx->DrawTextW(
      text.data(),
      static_cast<UINT32>(text.size()),
      textFormat,
      {mPadding, point.y, FLOAT(virtualSize.width), FLOAT(virtualSize.height)},
      footerBrush.get());
  }

  {
    auto text = winrt::to_hstring(fmt::format(
      fmt::runtime(_("Page {} of {}")),
      pageIndex + 1,
      std::max<uint16_t>(pageIndex + 1, GetPageCount())));

    textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    ctx->DrawTextW(
      text.data(),
      static_cast<UINT32>(text.size()),
      textFormat,
      {mPadding, point.y, virtualSize.width - mPadding, point.y + mRowHeight},
      footerBrush.get());
  }

  if (pageIndex + 1 < GetPageCount()) {
    std::wstring_view text(L">>>>>");

    textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    ctx->DrawTextW(
      text.data(),
      static_cast<UINT32>(text.size()),
      textFormat,
      {mPadding, point.y, virtualSize.width - mPadding, point.y + mRowHeight},
      footerBrush.get());
  }
}

void TabWithPlainTextContent::ClearText() {
  mMessages.clear();
  mCompletePages.clear();
  this->evFullyReplacedEvent.Emit();
}

void TabWithPlainTextContent::SetText(utf8_string_view text) {
  mMessages.clear();
  mCompletePages.clear();
  this->PushMessage(text);
  this->evFullyReplacedEvent.Emit();
}

void TabWithPlainTextContent::PushMessage(utf8_string_view message) {
  mMessages.push_back(utf8_string(message));
  LayoutMessages();
  this->evNeedsRepaintEvent.Emit();
}

void TabWithPlainTextContent::PushPage() {
  mCompletePages.push_back(mCurrentPageLines);
  mCurrentPageLines.clear();
  this->evPageAppendedEvent.Emit();
}

void TabWithPlainTextContent::LayoutMessages() {
  if (mRows <= 1 || mColumns <= 1) {
    return;
  }

  for (auto message: mMessages) {
    // tabs are variable width, and everything else here
    // assumes that all characters are the same width.
    //
    // Expand them
    while (true) {
      auto pos = message.find_first_of("\t");
      if (pos == message.npos) {
        break;
      }
      message.replace(pos, 1, "    ");
    }

    std::vector<std::string_view> rawLines;
    std::string_view remaining(message);
    while (!remaining.empty()) {
      auto newline = remaining.find_first_of("\n");
      if (newline == remaining.npos) {
        rawLines.push_back(remaining);
        break;
      }

      rawLines.push_back(remaining.substr(0, newline));
      if (remaining.size() <= newline) {
        break;
      }
      remaining = remaining.substr(newline + 1);
    }

    std::vector<std::string_view> wrappedLines;
    for (auto remaining: rawLines) {
      while (true) {
        if (remaining.size() <= mColumns) {
          wrappedLines.push_back(remaining);
          break;
        }

        auto space = remaining.find_last_of(" ", mColumns);
        if (space != remaining.npos) {
          wrappedLines.push_back(remaining.substr(0, space));
          if (remaining.size() <= space) {
            break;
          }
          remaining = remaining.substr(space + 1);
          continue;
        }

        wrappedLines.push_back(remaining.substr(0, mColumns));
        if (remaining.size() <= mColumns) {
          break;
        }
        remaining = remaining.substr(mColumns);
      }
    }

    if (wrappedLines.size() >= mRows) {
      if (!mCurrentPageLines.empty()) {
        mCurrentPageLines.push_back({});
      }

      for (const auto& line: wrappedLines) {
        if (mCurrentPageLines.size() >= mRows) {
          PushPage();
        }
        mCurrentPageLines.push_back(winrt::to_hstring(line));
      }
      continue;
    }

    // If we reach here, we can fit the full message on one page. Now figure
    // out if we want a new page first.
    if (mCurrentPageLines.empty()) {
      // do nothing
    } else if (mRows - mCurrentPageLines.size() >= wrappedLines.size() + 1) {
      // Add a blank line first
      mCurrentPageLines.push_back({});
    } else {
      // We need a new page
      PushPage();
    }

    for (auto line: wrappedLines) {
      mCurrentPageLines.push_back(winrt::to_hstring(line));
    }
  }
  mMessages.clear();
}

void TabWithPlainTextContent::PushFullWidthSeparator() {
  if (mColumns <= 0 || mMessages.empty()) {
    return;
  }
  this->PushMessage(std::string(mColumns, '-'));
}

}// namespace OpenKneeboard
