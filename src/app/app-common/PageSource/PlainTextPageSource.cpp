// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/D2DErrorRenderer.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/PlainTextPageSource.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <Unknwn.h>

#include <felly/numeric_cast.hpp>
#include <felly/overload.hpp>

#include <algorithm>
#include <format>

#include <dwrite.h>

namespace OpenKneeboard {

namespace {
using SourceReference = PlainTextPageSource::SourceReference;
// source is tracked separately as the content does not include trailing
// separators, e.g.:
// - \r\n or \n for lines
// - \n\n for paragraphs
// - \x1d (GROUP SEPARATOR) for groups
// The source *should* include these separators
struct WrappedLine {
  std::string_view mContent;
  SourceReference mSourceWithDelimiter;
  SourceReference mSourceWithoutDelimiter;

  operator std::string_view() const { return mContent; }
};
struct SourceLine {
  std::string_view mContent;
  std::vector<WrappedLine> mWrappedContent;
  SourceReference mSourceWithDelimiter;
  SourceReference mSourceWithoutDelimiter;

  void ApplyWordWrap(const std::size_t columns) {
    if (mContent.size() <= columns) {
      mWrappedContent = {
        WrappedLine {mContent, mSourceWithDelimiter, mSourceWithoutDelimiter}};
      return;
    }
    mWrappedContent.clear();
    auto remaining = mContent;
    while (!remaining.empty()) {
      const auto sourceLineOffset = remaining.data() - mContent.data();
      const auto sourceOffset = sourceLineOffset + mSourceWithDelimiter.mOffset;
      if (remaining.size() <= columns) {
        mWrappedContent.emplace_back(
          remaining,
          SourceReference {
            sourceOffset, mSourceWithDelimiter.mLength - sourceLineOffset},
          SourceReference {
            sourceOffset, mSourceWithoutDelimiter.mLength - sourceLineOffset});
        break;
      }

      const auto unbrokenMax = remaining.substr(0, columns);
      auto splitAt = std::ranges::find_last_if(
        unbrokenMax, [](const char c) { return std::isspace(c); });
      if (splitAt.empty()) {
        mWrappedContent.emplace_back(
          unbrokenMax,
          SourceReference {sourceOffset, columns},
          SourceReference {sourceOffset, columns});
        remaining = remaining.substr(columns);
        continue;
      }

      const std::string_view brokenLine {unbrokenMax.begin(), splitAt.begin()};
      const std::size_t sourceLengthWithDelimiter =
        std::ranges::distance(remaining.begin(), splitAt.end());
      mWrappedContent.emplace_back(
        brokenLine,
        SourceReference {sourceOffset, brokenLine.size()},
        SourceReference {sourceOffset, sourceLengthWithDelimiter});
      remaining = remaining.substr(sourceLengthWithDelimiter);
    }
  }
};

struct SourceParagraph {
  std::vector<SourceLine> mLines;
  SourceReference mSourceWithDelimiter;
  SourceReference mSourceWithoutDelimiter;
  std::size_t mWrappedLineCount {};

  void ApplyWordWrap(const std::size_t columns) {
    mWrappedLineCount = 0;
    for (auto&& line: mLines) {
      line.ApplyWordWrap(columns);
      mWrappedLineCount += line.mWrappedContent.size();
    }
  }
};

struct SourceGroup {
  std::vector<SourceParagraph> mParagraphs;
  SourceReference mSourceWithDelimiter;
  SourceReference mSourceWithoutDelimiter;

  std::size_t mWrappedLineCount {};

  void ApplyWordWrap(const std::size_t columns) {
    mWrappedLineCount = 0;
    for (auto&& paragraph: mParagraphs) {
      paragraph.ApplyWordWrap(columns);
      mWrappedLineCount += paragraph.mWrappedLineCount;
    }
  }
};
struct Source {
  std::vector<SourceGroup> mGroups;

  void ApplyWordWrap(const std::size_t columns) {
    for (auto&& group: mGroups) {
      group.ApplyWordWrap(columns);
    }
  }
};

void PopulateSourceParagraph(
  SourceParagraph& paragraph,
  const std::string_view allContent) {
  auto& lines = paragraph.mLines;
  const auto [paraOffset, paraLength] = paragraph.mSourceWithoutDelimiter;

  auto begin = paraOffset;
  const auto end = begin + paraLength;
  auto i = begin;
  while (i < end) {
    const auto remaining = allContent.substr(i);
    if (remaining.starts_with("\n")) {
      lines.push_back({
        .mSourceWithDelimiter = {begin, (i - begin) + 1},
        .mSourceWithoutDelimiter = {begin, (i - begin)},
      });
      i += 1;
      begin = i;
      continue;
    }
    if (remaining.starts_with("\r\n")) {
      lines.push_back({
        .mSourceWithDelimiter = {begin, (i - begin) + 2},
        .mSourceWithoutDelimiter = {begin, (i - begin)},
      });
      i += 2;
      begin = i;
      continue;
    }
    ++i;
  }
  if (begin < end) {
    lines.push_back({
      .mSourceWithDelimiter = {begin, (end - begin)},
      .mSourceWithoutDelimiter = {begin, (end - begin)},
    });
  }

  if (
    paragraph.mSourceWithoutDelimiter.mLength
    < paragraph.mSourceWithDelimiter.mLength) {
    OPENKNEEBOARD_ASSERT(
      paragraph.mSourceWithoutDelimiter.mOffset
      == paragraph.mSourceWithDelimiter.mOffset);
    const auto offset = paragraph.mSourceWithoutDelimiter.mOffset
      + paragraph.mSourceWithoutDelimiter.mLength;
    const auto length = paragraph.mSourceWithDelimiter.mLength
      - paragraph.mSourceWithoutDelimiter.mLength;
    lines.push_back({
      .mSourceWithDelimiter = {offset, length},
      .mSourceWithoutDelimiter = {offset, 0},
    });
  }
  if (lines.empty()) {
    return;
  }

  for (auto&& line: lines) {
    const auto [lineOffset, lineLength] = line.mSourceWithoutDelimiter;
    line.mContent =
      std::string_view {allContent}.substr(lineOffset, lineLength);
  }

  lines.back().mSourceWithDelimiter.mLength +=
    paragraph.mSourceWithDelimiter.mLength
    - paragraph.mSourceWithoutDelimiter.mLength;
}

void PopulateSourceGroup(
  SourceGroup& group,
  const std::string_view allContent) {
  auto& paragraphs = group.mParagraphs;
  const auto [groupOffset, groupLength] = group.mSourceWithoutDelimiter;

  auto begin = groupOffset;
  const auto end = begin + groupLength;
  auto i = begin;
  while (i < end) {
    const auto remaining = allContent.substr(i);
    if (remaining.starts_with("\n\n")) {
      paragraphs.push_back({
        .mSourceWithDelimiter = {begin, (i - begin) + 2},
        .mSourceWithoutDelimiter = {begin, (i - begin)},
      });
      i += 2;
      begin = i;
      continue;
    }
    if (remaining.starts_with("\r\n\r\n")) {
      paragraphs.push_back({
        .mSourceWithDelimiter = {begin, (i - begin) + 4},
        .mSourceWithoutDelimiter = {begin, (i - begin)},
      });
      i += 4;
      begin = i;
      continue;
    }
    ++i;
  }
  if (begin < end) {
    paragraphs.push_back({
      .mSourceWithDelimiter = {begin, (end - begin)},
      .mSourceWithoutDelimiter = {begin, (end - begin)},
    });
  }
  if (paragraphs.empty()) {
    return;
  }

  for (auto&& paragraph: paragraphs) {
    PopulateSourceParagraph(paragraph, allContent);
  }

  paragraphs.back().mSourceWithDelimiter.mLength +=
    group.mSourceWithDelimiter.mLength - group.mSourceWithoutDelimiter.mLength;
}

void PopulateSource(
  Source& source,
  const std::string_view allContent,
  const std::size_t offset) {
  static constexpr auto GroupSeparator = '\x1d';
  auto& groups = source.mGroups;
  std::size_t begin = offset;
  std::size_t sep = begin;
  do {
    sep = allContent.find(GroupSeparator, begin);
    if (sep == allContent.npos) {
      break;
    }
    groups.push_back({
      .mSourceWithDelimiter = {begin, (sep - begin) + 1},
      .mSourceWithoutDelimiter = {begin, (sep - begin)},
    });
    begin = sep + 1;
  } while (sep != allContent.npos);

  if (begin < allContent.size()) {
    groups.push_back({
      .mSourceWithDelimiter = {begin, allContent.size() - begin},
      .mSourceWithoutDelimiter = {begin, allContent.size() - begin},
    });
  }

  for (auto&& group: groups) {
    PopulateSourceGroup(group, allContent);
  }
}

}// namespace

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

  // With a font/size change, may no longer correlate
  mPageIDs.clear();
  mPages.clear();

  UpdateLayout();
}

PageIndex PlainTextPageSource::GetPageCount() const {
  if (mPages.empty()) {
    return mPlaceholderText.empty() ? 0 : 1;
  }
  return mPages.size();
}

std::vector<PageID> PlainTextPageSource::GetPageIDs() const {
  mPageIDs.resize(GetPageCount());
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
  if (mPages.empty()) {
    if (mPlaceholderText.empty()) {
      co_return;
    }
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

  const auto& lines = mPages.at(*pageIndex).mLines;

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
    static constexpr std::wstring_view text(L"<<<<<");
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
    static constexpr std::wstring_view text(L">>>>>");

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
  return mPages.empty() || mPages.front().mLines.empty();
}

void PlainTextPageSource::ClearText() {
  {
    std::unique_lock lock(mMutex);
    if (IsEmpty()) {
      return;
    }
    mPages.clear();
    mPageIDs.clear();
  }
  this->evContentChangedEvent.Emit();
}

void PlainTextPageSource::SetText(std::string_view text) {
  {
    std::unique_lock lock(mMutex);
    if (mContent == text) {
      return;
    }
    mContent = text;
    mFirstModifiedOffset = unknown_offset;
  }
  this->UpdateLayout();
  evContentChangedEvent.Emit();
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

  SetText(std::format("{}\x1d{}", mContent, sMessage));
}

void PlainTextPageSource::EnsureNewPage() {
  std::unique_lock lock(mMutex);
  if (mPages.empty()) {
    return;
  }
  if (!mPages.back().mLines.empty()) {
    this->PushPage();
  }
}

void PlainTextPageSource::PushPage() {
  const auto offset = mPages.empty()
    ? 0
    : (mPages.back().mSource.mOffset + mPages.back().mSource.mLength);
  mPages.push_back({});
  mPages.back().mSource.mOffset = offset;
  this->evPageAppendedEvent.Emit(SuggestedPageAppendAction::SwitchToNewPage);
}

void PlainTextPageSource::AppendContent(const std::string_view append) {
  const auto previousSize = mContent.size();
  mContent += append;
  using first_offset_t = decltype(mFirstModifiedOffset);
  mFirstModifiedOffset = std::visit<first_offset_t>(
    felly::overload {
      [=](std::monostate) { return previousSize; },
      [=](const std::size_t previousOffset) {
        OPENKNEEBOARD_ASSERT(previousOffset <= previousSize);
        return previousOffset;
      },
      [](unknown_offset_t) { return unknown_offset; },
    },
    mFirstModifiedOffset);
}

void PlainTextPageSource::ReplaceContent(const std::string_view replacement) {
  mContent = replacement;
  mFirstModifiedOffset = unknown_offset;
}

void PlainTextPageSource::PushFullWidthSeparator() {
  std::unique_lock lock(mMutex);
  if (mColumns <= 0 || mPages.empty() || mPages.back().mLines.empty()) {
    return;
  }
  this->PushMessage(std::string(mColumns, '-'));
}

void PlainTextPageSource::UpdateLayout() {
  if (mContent == mLastLayoutContent) {
    return;
  }

  ///// 1. Find where modifications start /////
  const auto firstModifiedOffset = std::visit<std::size_t>(
    felly::overload {
      [=](std::monostate) -> std::size_t {
        fatal("Have differing text without a first modified offset");
      },
      [=](const std::size_t offset) { return offset; },
      [this](unknown_offset_t) {
        const auto it =
          std::mismatch(
            mContent.begin(),
            mContent.begin()
              + std::min(mContent.size(), mLastLayoutContent.size()),
            mLastLayoutContent.begin())
            .first;
        return std::distance(mContent.begin(), it);
      },
    },
    std::exchange(mFirstModifiedOffset, {}));
  mLastLayoutContent = mContent;
  const auto firstDifferingPage =
    std::ranges::find_if(mPages, [firstModifiedOffset](const auto& page) {
      return page.mSource.mOffset + page.mSource.mLength > firstModifiedOffset;
    });
  mPages.erase(firstDifferingPage, mPages.end());
  mPageIDs.resize(mPages.size());
  if (firstModifiedOffset == mContent.size()) {
    return;
  }

  ///// 2. Split the input into groups, paragraphs, and lines /////
  OPENKNEEBOARD_ASSERT(mRows > 1 && mColumns > 1);
  const auto repaintAtEnd =
    scope_exit {[this] { evContentChangedEvent.Emit(); }};
  Source source {};
  if (mPages.empty()) {
    PopulateSource(source, mContent, 0);
  } else {
    const auto& [offset, length] = mPages.back().mSource;
    PopulateSource(source, mContent, offset + length);
  }

  source.ApplyWordWrap(mColumns);

  ///// 3. Add to pages /////
  bool haveNewPage = false;
  const auto emitNewPage = scope_exit([&, this] {
    if (haveNewPage) {
      evPageAppendedEvent.Emit(SuggestedPageAppendAction::SwitchToNewPage);
    }
  });
  if (mPages.empty()) {
    mPages.emplace_back();
    haveNewPage = true;
  }
  auto remainingRows = mRows - mPages.back().mLines.size();
  const auto appendLinesToCurrentPage = [&, this](auto&& sourceLines) {
    if (sourceLines.empty()) {
      return;
    }

    auto& page = mPages.back();
    const auto wrappedLines = sourceLines
      | std::views::transform(&SourceLine::mWrappedContent) | std::views::join;
    page.mLines.append_range(
      wrappedLines | std::views::transform([](const auto& utf8) {
        return Win32::UTF8::or_throw::to_wide(utf8);
      }));
    const auto [lastStart, lastLength] =
      sourceLines.back().mSourceWithDelimiter;
    const auto end = lastStart + lastLength;
    page.mSource.mLength = end - page.mSource.mOffset;
    remainingRows -= std::ranges::distance(wrappedLines);
  };

  const auto pushPage = [&, this](const std::size_t offset) {
    mPages.emplace_back();
    mPages.back().mSource = {offset, 0};
    remainingRows = mRows;
    haveNewPage = true;
  };

  for (auto&& group: source.mGroups) {
    if (remainingRows == 0) {
      pushPage(group.mSourceWithDelimiter.mOffset);
    }

    if (group.mWrappedLineCount <= mRows) {
      if (group.mWrappedLineCount > remainingRows) {
        pushPage(group.mSourceWithDelimiter.mOffset);
      }
      appendLinesToCurrentPage(
        group.mParagraphs | std::views::transform(&SourceParagraph::mLines)
        | std::views::join);
      continue;
    }

    for (auto&& paragraph: group.mParagraphs) {
      if (paragraph.mWrappedLineCount <= mRows) {
        if (paragraph.mWrappedLineCount > remainingRows) {
          pushPage(paragraph.mSourceWithDelimiter.mOffset);
        }
        appendLinesToCurrentPage(paragraph.mLines);
        continue;
      }

      for (auto&& line: paragraph.mLines) {
        const auto wrappedLines = line.mWrappedContent.size();
        if (wrappedLines <= mRows) {
          if (wrappedLines > remainingRows) {
            pushPage(line.mSourceWithDelimiter.mOffset);
          }
          appendLinesToCurrentPage(std::views::single(line));
          continue;
        }
        auto remaining = std::span {line.mWrappedContent};
        while (!remaining.empty()) {
          if (remainingRows == 0) {
            pushPage(remaining.front().mSourceWithDelimiter.mOffset);
          }

          const auto count = std::min(remainingRows, remaining.size());
          const auto chunk =
            remaining.first(count) | std::ranges::to<std::vector>();
          const auto& back = chunk.back();
          const auto offset = chunk.front().mSourceWithDelimiter.mOffset;
          const SourceLine it {
            .mWrappedContent =
              remaining.first(count) | std::ranges::to<std::vector>(),
            .mSourceWithDelimiter =
              {offset,
               back.mSourceWithDelimiter.mOffset
                 + back.mSourceWithDelimiter.mLength - offset},
            .mSourceWithoutDelimiter =
              {offset,
               back.mSourceWithoutDelimiter.mOffset
                 + back.mSourceWithoutDelimiter.mLength - offset},
          };
          appendLinesToCurrentPage(std::views::single(it));
          remaining = remaining.subspan(count);
        }
      }
    }
  }
}

}// namespace OpenKneeboard
