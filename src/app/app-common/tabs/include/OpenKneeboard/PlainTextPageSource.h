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
  PlainTextPageSource(const DXResources&, utf8_string_view placeholderText);
  virtual ~PlainTextPageSource();

  void ClearText();
  void SetText(utf8_string_view text);
  void PushMessage(utf8_string_view message);
  void PushFullWidthSeparator();
  void EnsureNewPage();

  virtual uint16_t GetPageCount() const override;
  virtual D2D1_SIZE_U GetNativeContentSize(uint16_t pageIndex) override;
  virtual void RenderPage(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F& rect) override;

 private:
  static constexpr int RENDER_SCALE = 1;

  mutable std::recursive_mutex mMutex;
  std::vector<std::vector<winrt::hstring>> mCompletePages;
  std::vector<winrt::hstring> mCurrentPageLines;
  std::vector<utf8_string> mMessages;

  float mPadding = -1.0f;
  float mRowHeight = -1.0f;
  int mColumns = -1;
  int mRows = -1;

  DXResources mDXR;
  winrt::com_ptr<IDWriteTextFormat> mTextFormat;
  utf8_string mPlaceholderText;

  void LayoutMessages();
  void PushPage();
};

}// namespace OpenKneeboard
