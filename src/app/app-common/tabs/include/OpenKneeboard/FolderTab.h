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

#include <shims/winrt.h>
#include <winrt/Windows.Storage.Search.h>

#include <shims/filesystem>

#include "ITab.h"
#include "ITabWithNavigation.h"
#include "ITabWithSettings.h"
#include "TabBase.h"
#include "TabWithDoodles.h"

namespace OpenKneeboard {

class ImagePageSource;

class FolderTab final : public TabBase,
                        public TabWithDoodles,
                        public ITabWithNavigation,
                        public ITabWithSettings {
 public:
  FolderTab(
    const DXResources&,
    KneeboardState*,
    utf8_string_view title,
    const std::filesystem::path& path);
  explicit FolderTab(
    const DXResources&,
    KneeboardState*,
    utf8_string_view title,
    const nlohmann::json&);
  virtual ~FolderTab();
  virtual utf8_string GetGlyph() const override;
  virtual utf8_string GetTitle() const override;

  virtual nlohmann::json GetSettings() const final override;

  virtual void Reload() final override;
  virtual uint16_t GetPageCount() const final override;
  virtual D2D1_SIZE_U GetNativeContentSize(uint16_t pageIndex) final override;

  std::filesystem::path GetPath() const;
  virtual void SetPath(const std::filesystem::path& path);

  virtual bool IsNavigationAvailable() const override;
  virtual std::shared_ptr<ITab> CreateNavigationTab(uint16_t) override;

 protected:
  virtual void RenderPageContent(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F& rect) final override;

 private:
  winrt::apartment_context mUIThread;

  DXResources mDXR;

  std::unique_ptr<ImagePageSource> mPageSource;
  std::filesystem::path mPath;

  winrt::Windows::Storage::Search::StorageFileQueryResult mQueryResult {
    nullptr};

  winrt::fire_and_forget ReloadImpl() noexcept;
};

}// namespace OpenKneeboard
