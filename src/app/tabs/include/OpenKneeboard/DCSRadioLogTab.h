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

#include "DCSTab.h"
#include "TabWithDoodles.h"

namespace OpenKneeboard {

class FolderTab;

class DCSRadioLogTab final : public DCSTab, public TabWithDoodles {
 public:
  DCSRadioLogTab(const DXResources&);
  virtual ~DCSRadioLogTab();
  virtual utf8_string GetTitle() const override;

  virtual void Reload() override;
  virtual uint16_t GetPageCount() const override;
  virtual D2D1_SIZE_U GetNativeContentSize(uint16_t pageIndex) override;

 protected:
  virtual void RenderPageContent(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F& rect) override;

  void PushMessage(utf8_string_view message);

  virtual const char* GetGameEventName() const override;
  virtual void Update(
    const std::filesystem::path& installPath,
    const std::filesystem::path& savedGamesPath,
    utf8_string_view value) override;

  virtual void OnSimulationStart() override;

 private:
  static constexpr int RENDER_SCALE = 1;

  std::vector<std::vector<winrt::hstring>> mCompletePages;
  std::vector<winrt::hstring> mCurrentPageLines;
  std::vector<utf8_string> mMessages;

  float mPadding = -1.0f;
  float mRowHeight = -1.0f;
  int mColumns = -1;
  int mRows = -1;

  DXResources mDXR;
  winrt::com_ptr<IDWriteTextFormat> mTextFormat;

  void LayoutMessages();
  void PushPage();
};

}// namespace OpenKneeboard
