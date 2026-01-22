// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/D2DErrorRenderer.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/PlainTextPageSource.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <Unknwn.h>

#include <felly/numeric_cast.hpp>

#include <algorithm>
#include <format>

#include <dwrite.h>

namespace OpenKneeboard {

PlainTextPageSource::PlainTextPageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  std::string_view placeholderText)
  : mDXR(dxr),
    mKneeboard(kbs),
    mPlaceholderText(placeholderText) {
  mFontSize = kbs->GetTextSettings().mFontSize;

  auto dwf = mDXR->mDWriteFactory;
  dwf->CreateTextFormat(
    FixedWidthContentFont,
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    mFontSize,
    L"",
    mTextFormat.put());

  UpdateLayoutLimits();

  // subscribe to settings changed events
  AddEventListener(
    kbs->evSettingsChangedEvent,
    std::bind_front(&PlainTextPageSource::OnSettingsChanged, this));
}

void PlainTextPageSource::UpdateLayoutLimits() {
  auto dwf = mDXR->mDWriteFactory;

  const auto size = this->GetPreferredSize({nullptr})->mPixelSize;
  winrt::com_ptr<IDWriteTextLayout> textLayout;
  dwf->CreateTextLayout(
    L"m",
    1,
    mTextFormat.get(),
    size.Width<FLOAT>(),
    size.Height<FLOAT>(),
    textLayout.put());
  DWRITE_TEXT_METRICS metrics;
  textLayout->GetMetrics(&metrics);

  mPadding = mRowHeight = metrics.height;
  mRows =
    static_cast<int>((size.mHeight - (2 * mPadding)) / metrics.height) - 2;
  mColumns = static_cast<int>((size.mWidth - (2 * mPadding)) / metrics.width);
}

PlainTextPageSource::~PlainTextPageSource() { this->RemoveAllEventListeners(); }

void PlainTextPageSource::OnSettingsChanged() {
  auto newFontSize = mKneeboard->GetTextSettings().mFontSize;

  if (newFontSize == mFontSize) {
    return;
  }

  mFontSize = newFontSize;

  auto dwf = mDXR->mDWriteFactory;

  mTextFormat = nullptr;

  dwf->CreateTextFormat(
    FixedWidthContentFont,
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    newFontSize,
    L"",
    mTextFormat.put());

  UpdateLayoutLimits();

  // now clear and redraw all pages:
  mCompletePages.clear();
  mCurrentPageLines.clear();
  mPageIDs.clear();

  mMessagesToLayout.clear();
  for (const std::string& m: mAllMessages) {
    mMessagesToLayout.push_back(m);
  }
  LayoutMessages();
}

PageIndex PlainTextPageSource::GetPageCount() const {
  if (mCompletePages.empty() && mCurrentPageLines.empty()) {
    return mPlaceholderText.empty() ? 0 : 1;
  }

  // We only push a complete page when there's content (or about to be)
  return mCompletePages.size() + 1;
}

std::vector<PageID> PlainTextPageSource::GetPageIDs() const {
  const auto pageCount = this->GetPageCount();
  if (mPageIDs.size() < pageCount) {
    mPageIDs.resize(pageCount);
    if (TraceLoggingProviderEnabled(gTraceProvider, 0, 0)) {
      std::vector<uint64_t> values(pageCount);
      std::ranges::transform(
        mPageIDs, begin(values), &PageID::GetTemporaryValue);
      TraceLoggingWrite(
        gTraceProvider,
        "PlainTextPageSource::GetPageIDs()",
        TraceLoggingHexUInt64Array(values.data(), values.size(), "PageIDs"));
    }
  }
  return mPageIDs;
}

std::optional<PreferredSize> PlainTextPageSource::GetPreferredSize(PageID) {
  return PreferredSize {
    DefaultPixelSize,
    ScalingKind::Vector,
  };
}

std::optional<PageIndex> PlainTextPageSource::FindPageIndex(
  PageID pageID) const {
  auto it = std::ranges::find(mPageIDs, pageID);
  if (it == mPageIDs.end()) {
    return {};
  }
  return {static_cast<PageIndex>(it - mPageIDs.begin())};
}

task<void> PlainTextPageSource::RenderPage(
  RenderContext rc,
  PageID pageID,
  PixelRect rect) {
  std::unique_lock lock(mMutex);

  const auto virtualSize = this->GetPreferredSize(pageID)->mPixelSize;
  const auto renderSize = virtualSize.ScaledToFit(rect.mSize);

  const auto renderLeft = felly::numeric_cast<float>(
    rect.Left() + ((rect.Width() - renderSize.Width()) / 2));
  const float renderTop = felly::numeric_cast<float>(
    rect.Top() + ((rect.Height() - renderSize.Height()) / 2));

  const auto scale = renderSize.Height<float>() / virtualSize.Height();

  auto ctx = rc.d2d();
  ctx->SetTransform(
    D2D1::Matrix3x2F::Scale(scale, scale)
    * D2D1::Matrix3x2F::Translation(renderLeft, renderTop));

  winrt::com_ptr<ID2D1SolidColorBrush> background;
  winrt::com_ptr<ID2D1SolidColorBrush> textBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> footerBrush;
  ctx->CreateSolidColorBrush({1.0f, 1.0f, 1.0f, 1.0f}, background.put());
  ctx->CreateSolidColorBrush({0.0f, 0.0f, 0.0f, 1.0f}, textBrush.put());
  ctx->CreateSolidColorBrush({0.5f, 0.5f, 0.5f, 1.0f}, footerBrush.put());

  ctx->FillRectangle(
    {
      0.0f,
      0.0f,
      virtualSize.Width<float>(),
      virtualSize.Height<float>(),
    },
    background.get());

  auto textFormat = mTextFormat.get();
  textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  if (mCurrentPageLines.empty()) {
    auto message = winrt::to_hstring(mPlaceholderText);
    ctx->DrawTextW(
      message.data(),
      static_cast<UINT32>(message.size()),
      textFormat,
      {mPadding,
       mPadding,
       virtualSize.mWidth - mPadding,
       mPadding + mRowHeight},
      footerBrush.get());
    co_return;
  }

  const auto pageIndex = FindPageIndex(pageID);

  if (!pageIndex) [[unlikely]] {
    D2DErrorRenderer(mDXR).Render(ctx, _("Invalid Page ID"), rect);
    co_return;
  }

  const auto& lines = (*pageIndex == mCompletePages.size())
    ? mCurrentPageLines
    : mCompletePages.at(*pageIndex);

  D2D_POINT_2F point {mPadding, mPadding};
  for (const auto& line: lines) {
    ctx->DrawTextW(
      line.data(),
      static_cast<UINT32>(line.size()),
      textFormat,
      {point.x, point.y, virtualSize.mWidth - point.x, point.y + mRowHeight},
      textBrush.get());
    point.y += mRowHeight;
  }

  point.y = virtualSize.mHeight - (mRowHeight + mPadding);

  if (*pageIndex > 0) {
    std::wstring_view text(L"<<<<<");
    ctx->DrawTextW(
      text.data(),
      static_cast<UINT32>(text.size()),
      textFormat,
      {
        mPadding,
        point.y,
        virtualSize.Width<FLOAT>(),
        virtualSize.Height<FLOAT>(),
      },
      footerBrush.get());
  }

  {
    auto text = std::format(
      _(L"Page {} of {}"),
      *pageIndex + 1,
      std::max<PageIndex>(*pageIndex + 1, GetPageCount()));

    textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    ctx->DrawTextW(
      text.data(),
      static_cast<UINT32>(text.size()),
      textFormat,
      {mPadding, point.y, virtualSize.mWidth - mPadding, point.y + mRowHeight},
      footerBrush.get());
  }

  if (*pageIndex + 1 < GetPageCount()) {
    std::wstring_view text(L">>>>>");

    textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    ctx->DrawTextW(
      text.data(),
      static_cast<UINT32>(text.size()),
      textFormat,
      {mPadding, point.y, virtualSize.mWidth - mPadding, point.y + mRowHeight},
      footerBrush.get());
  }
}

bool PlainTextPageSource::IsEmpty() const {
  std::unique_lock lock(mMutex);
  return mMessagesToLayout.empty() && mCurrentPageLines.empty();
}

void PlainTextPageSource::ClearText() {
  {
    std::unique_lock lock(mMutex);
    if (IsEmpty()) {
      return;
    }
    mMessagesToLayout.clear();
    mAllMessages.clear();
    mCurrentPageLines.clear();
    mCompletePages.clear();
    mPageIDs.clear();
  }
  this->evContentChangedEvent.Emit();
}

void PlainTextPageSource::SetText(std::string_view text) {
  std::unique_lock lock(mMutex);
  this->ClearText();
  this->PushMessage(text);
}

void PlainTextPageSource::SetPlaceholderText(std::string_view text) {
  if (std::string_view {text} == mPlaceholderText) {
    return;
  }
  mPlaceholderText = std::string {text};
  if (IsEmpty()) {
    this->evContentChangedEvent.Emit();
  }
}

void PlainTextPageSource::PushMessage(std::string_view message) {
  std::unique_lock lock(mMutex);

  auto sMessage = std::string(message);

  // tabs are variable width, and everything else here
  // assumes that all characters are the same width.
  //
  // Expand them
  while (true) {
    auto pos = sMessage.find_first_of("\t");
    if (pos == sMessage.npos) {
      break;
    }
    sMessage.replace(pos, 1, "    ");
  }

  mAllMessages.push_back(sMessage);
  mMessagesToLayout.push_back(mAllMessages.back());
  LayoutMessages();
}

void PlainTextPageSource::EnsureNewPage() {
  std::unique_lock lock(mMutex);
  if (!mCurrentPageLines.empty()) {
    this->PushPage();
  }
}

void PlainTextPageSource::PushPage() {
  mCompletePages.push_back(mCurrentPageLines);
  mCurrentPageLines.clear();
  this->evPageAppendedEvent.Emit(SuggestedPageAppendAction::SwitchToNewPage);
}

void PlainTextPageSource::LayoutMessages() {
  if (mRows <= 1 || mColumns <= 1) {
    return;
  }

  if (mMessagesToLayout.empty()) {
    return;
  }

  const scope_exit repaintAtEnd(
    [this]() { this->evContentChangedEvent.Emit(); });

  for (auto message: mMessagesToLayout) {
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
    for (auto&& line: rawLines) {
      while (true) {
        if (line.size() <= mColumns) {
          wrappedLines.push_back(line);
          break;
        }

        auto space = line.find_last_of(" ", mColumns);
        if (space != line.npos) {
          wrappedLines.push_back(line.substr(0, space));
          if (line.size() <= space) {
            break;
          }
          line = line.substr(space + 1);
          continue;
        }

        wrappedLines.push_back(line.substr(0, mColumns));
        if (line.size() <= mColumns) {
          break;
        }
        line = line.substr(mColumns);
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
  mMessagesToLayout.clear();
}

void PlainTextPageSource::PushFullWidthSeparator() {
  std::unique_lock lock(mMutex);
  if (
    mColumns <= 0 || (mMessagesToLayout.empty() && mCurrentPageLines.empty())) {
    return;
  }
  this->PushMessage(std::string(mColumns, '-'));
}

}// namespace OpenKneeboard
