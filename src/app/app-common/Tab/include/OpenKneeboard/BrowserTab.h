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

#include <OpenKneeboard/PageSourceWithDelegates.h>
#include <OpenKneeboard/TabBase.h>
#include <OpenKneeboard/WebView2PageSource.h>

#include <OpenKneeboard/audited_ptr.h>
#include <OpenKneeboard/handles.h>

namespace OpenKneeboard {

class HWNDPageSource;

class BrowserTab final : public TabBase,
                         public PageSourceWithDelegates,
                         public std::enable_shared_from_this<BrowserTab> {
 public:
  BrowserTab() = delete;
  BrowserTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title);
  virtual ~BrowserTab();

  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  virtual void Reload() final override;

 private:
  winrt::apartment_context mUIThread;
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard {nullptr};
  std::shared_ptr<WebView2PageSource> mDelegate;
};

}// namespace OpenKneeboard
