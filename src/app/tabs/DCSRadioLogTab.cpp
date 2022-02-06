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
#include <OpenKneeboard/DCSRadioLogTab.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/Games/DCSWorld.h>
#include <OpenKneeboard/dprint.h>
#include <Unknwn.h>
#include <dwrite.h>
#include <fmt/format.h>
#include <fmt/xchar.h>

#include <algorithm>

#include "okEvents.h"

using DCS = OpenKneeboard::Games::DCSWorld;

namespace OpenKneeboard {

class DCSRadioLogTab::Impl final {
 private:
  DCSRadioLogTab* mTab = nullptr;

 public:
  static const int RENDER_SCALE = 1;

  Impl() = delete;
  Impl(DCSRadioLogTab* tab, const DXResources& dxr);
  ~Impl();

  std::vector<std::vector<std::wstring>> mCompletePages;
  std::vector<std::wstring> mCurrentPageLines;
  std::vector<std::wstring> mMessages;

  float mPadding = -1.0f;
  float mRowHeight = -1.0f;
  int mColumns = -1;
  int mRows = -1;

  DXResources mDXR;
  winrt::com_ptr<IDWriteTextFormat> mTextFormat;

  void PushMessage(const std::string& message);
  void LayoutMessages();
  void PushPage();
};

DCSRadioLogTab::DCSRadioLogTab(const DXResources& dxr)
  : DCSTab(dxr, _("Radio Log")), p(std::make_unique<Impl>(this, dxr)) {
}

DCSRadioLogTab::~DCSRadioLogTab() {
}

void DCSRadioLogTab::Reload() {
}

uint16_t DCSRadioLogTab::GetPageCount() const {
  if (p->mCompletePages.empty()) {
    return 1;
  }

  if (p->mCurrentPageLines.empty()) {
    return p->mCompletePages.size();
  }

  return p->mCompletePages.size() + 1;
}

D2D1_SIZE_U DCSRadioLogTab::GetNativeContentSize(uint16_t pageIndex) {
  return {768 * Impl::RENDER_SCALE, 1024 * Impl::RENDER_SCALE};
}

void DCSRadioLogTab::RenderPageContent(
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {

  const auto virtualSize = GetNativeContentSize(0);
  const D2D1_SIZE_F canvasSize {rect.right - rect.left, rect.bottom - rect.top};

  const auto scaleX = canvasSize.width / virtualSize.width;
  const auto scaleY = canvasSize.height / virtualSize.height;
  const auto scale = std::min(scaleX, scaleY);
  const D2D1_SIZE_F renderSize {
    scale * virtualSize.width, scale * virtualSize.height};

  auto ctx = p->mDXR.mD2DDeviceContext;
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

  auto textFormat = p->mTextFormat.get();
  textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  if (p->mCurrentPageLines.empty()) {
    auto message = _("[waiting for radio messages]").ToStdWstring();
    ctx->DrawTextW(
      message.data(),
      static_cast<UINT32>(message.size()),
      textFormat,
      {p->mPadding,
       p->mPadding,
       virtualSize.width - p->mPadding,
       p->mPadding + p->mRowHeight},
      footerBrush.get());
    return;
  }
  const auto& lines = (pageIndex == p->mCompletePages.size())
    ? p->mCurrentPageLines
    : p->mCompletePages.at(pageIndex);

  D2D_POINT_2F point {p->mPadding, p->mPadding};
  for (const auto& line: lines) {
    ctx->DrawTextW(
      line.data(),
      static_cast<UINT32>(line.size()),
      textFormat,
      {point.x, point.y, virtualSize.width - point.x, point.y + p->mRowHeight},
      textBrush.get());
    point.y += p->mRowHeight;
  }

  point.y = virtualSize.height - (p->mRowHeight + p->mPadding);

  if (pageIndex > 0) {
    std::wstring text(L"<<<<<");
    ctx->DrawTextW(
      text.data(),
      static_cast<UINT32>(text.size()),
      textFormat,
      {p->mPadding,
       point.y,
       FLOAT(virtualSize.width),
       FLOAT(virtualSize.height)},
      footerBrush.get());
  }

  {
    auto text = fmt::format(
      _("Page {} of {}").ToStdWstring(), pageIndex + 1, GetPageCount());

    textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    ctx->DrawTextW(
      text.data(),
      static_cast<UINT32>(text.size()),
      textFormat,
      {p->mPadding,
       point.y,
       virtualSize.width - p->mPadding,
       point.y + p->mRowHeight},
      footerBrush.get());
  }

  if (pageIndex + 1 < GetPageCount()) {
    std::wstring text(L">>>>>");

    textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    ctx->DrawTextW(
      text.data(),
      static_cast<UINT32>(text.size()),
      textFormat,
      {p->mPadding,
       point.y,
       virtualSize.width - p->mPadding,
       point.y + p->mRowHeight},
      footerBrush.get());
  }
}

void DCSRadioLogTab::Impl::PushMessage(const std::string& message) {
  mMessages.push_back(std::wstring(winrt::to_hstring(message)));
  LayoutMessages();
  mTab->evNeedsRepaintEvent();
}

void DCSRadioLogTab::Impl::PushPage() {
  mCompletePages.push_back(mCurrentPageLines);
  mCurrentPageLines.clear();

  mTab->evPageAppendedEvent();
  mTab->evNeedsRepaintEvent();
}

void DCSRadioLogTab::Impl::LayoutMessages() {
  if (mRows <= 1 || mColumns <= 1) {
    return;
  }

  for (const auto& message: mMessages) {
    std::vector<std::wstring_view> rawLines;
    std::wstring_view remaining(message);
    while (!remaining.empty()) {
      auto newline = remaining.find_first_of(L"\n");
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

    std::vector<std::wstring_view> wrappedLines;
    for (auto remaining: rawLines) {
      while (true) {
        if (remaining.size() <= mColumns) {
          wrappedLines.push_back(remaining);
          break;
        }

        auto space = remaining.find_last_of(L" ", mColumns);
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
        mCurrentPageLines.push_back(std::wstring(line));
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
      mCurrentPageLines.push_back(std::wstring(line));
    }
  }
  mMessages.clear();
}

const char* DCSRadioLogTab::GetGameEventName() const {
  return DCS::EVT_RADIO_MESSAGE;
}

void DCSRadioLogTab::Update(
  const std::filesystem::path& installPath,
  const std::filesystem::path& savedGamesPath,
  const std::string& value) {
  p->PushMessage(value);
}

void DCSRadioLogTab::OnSimulationStart() {
  if (p->mColumns <= 0 || p->mMessages.empty()) {
    return;
  }
  p->PushMessage(std::string(p->mColumns, '-'));
}

DCSRadioLogTab::Impl::Impl(DCSRadioLogTab* tab, const DXResources& dxr)
  : mTab(tab), mDXR(dxr) {
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

  const auto size = tab->GetNativeContentSize(0);
  winrt::com_ptr<IDWriteTextLayout> textLayout;
  dwf->CreateTextLayout(
    L"m", 1, mTextFormat.get(), size.width, size.height, textLayout.put());
  DWRITE_TEXT_METRICS metrics;
  textLayout->GetMetrics(&metrics);

  mPadding = mRowHeight = metrics.height;
  mRows = static_cast<int>((size.height - (2 * mPadding)) / metrics.height) - 2;
  mColumns = static_cast<int>((size.width - (2 * mPadding)) / metrics.width);
}

DCSRadioLogTab::Impl::~Impl() {
}

}// namespace OpenKneeboard
