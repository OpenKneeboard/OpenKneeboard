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

#include "IPageSource.hpp"

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/KneeboardState.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <memory>
#include <mutex>

namespace OpenKneeboard {

struct DXResources;

class PlainTextPageSource final : public IPageSource,
                                  public virtual EventReceiver {
 public:
  PlainTextPageSource() = delete;
  PlainTextPageSource(
    const audited_ptr<DXResources>&,
    KneeboardState* kbs,
    std::string_view placeholderText);
  virtual ~PlainTextPageSource();
  void OnSettingsChanged();
  bool IsEmpty() const;
  void ClearText();
  void SetText(std::string_view text);
  void SetPlaceholderText(std::string_view text);
  void PushMessage(std::string_view message);
  void PushFullWidthSeparator();
  void EnsureNewPage();

  virtual PageIndex GetPageCount() const override;
  virtual std::vector<PageID> GetPageIDs() const override;
  virtual PreferredSize GetPreferredSize(PageID) override;
  task<void>
  RenderPage(RenderContext, PageID, PixelRect rect) override;

 private:
  static constexpr int RENDER_SCALE = 1;

  mutable std::recursive_mutex mMutex;
  mutable std::vector<PageID> mPageIDs;
  std::vector<std::vector<winrt::hstring>> mCompletePages;
  std::vector<winrt::hstring> mCurrentPageLines;
  std::vector<std::string_view> mMessagesToLayout;
  std::vector<std::string> mAllMessages;

  std::optional<PageIndex> FindPageIndex(PageID) const;

  float mPadding = -1.0f;
  float mRowHeight = -1.0f;
  int mColumns = -1;
  int mRows = -1;
  float mFontSize;

  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard;
  winrt::com_ptr<IDWriteTextFormat> mTextFormat;
  std::string mPlaceholderText;

  void UpdateLayoutLimits();
  void LayoutMessages();
  void PushPage();
};

}// namespace OpenKneeboard
