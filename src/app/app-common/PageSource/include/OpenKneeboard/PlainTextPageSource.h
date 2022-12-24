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

#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/utf8.h>
#include <shims/winrt/base.h>

#include <memory>
#include <mutex>

#include "IPageSource.h"

namespace OpenKneeboard {

struct DXResources;

class PlainTextPageSource final : public IPageSource {
 public:
  PlainTextPageSource() = delete;
  PlainTextPageSource(const DXResources&, std::string_view placeholderText);
  virtual ~PlainTextPageSource();

  bool IsEmpty() const;
  void ClearText();
  void SetText(std::string_view text);
  void SetPlaceholderText(std::string_view text);
  void PushMessage(std::string_view message);
  void PushFullWidthSeparator();
  void EnsureNewPage();

  virtual PageIndex GetPageCount() const override;
  virtual D2D1_SIZE_U GetNativeContentSize(PageIndex pageIndex) override;
  virtual void RenderPage(
    ID2D1DeviceContext*,
    PageIndex pageIndex,
    const D2D1_RECT_F& rect) override;

 private:
  static constexpr int RENDER_SCALE = 1;

  mutable std::recursive_mutex mMutex;
  std::vector<std::vector<winrt::hstring>> mCompletePages;
  std::vector<winrt::hstring> mCurrentPageLines;
  std::vector<std::string> mMessages;

  float mPadding = -1.0f;
  float mRowHeight = -1.0f;
  int mColumns = -1;
  int mRows = -1;

  DXResources mDXR;
  winrt::com_ptr<IDWriteTextFormat> mTextFormat;
  std::string mPlaceholderText;

  void LayoutMessages();
  void PushPage();
};

}// namespace OpenKneeboard
