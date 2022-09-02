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
#include <shims/winrt/base.h>
#include <winrt/Windows.Storage.Search.h>

#include <shims/filesystem>

#include "ITab.h"
#include "ITabWithSettings.h"
#include "TabBase.h"

namespace OpenKneeboard {

class PlainTextPageSource;

class TextFileTab final : public TabBase,
                          public ITabWithSettings,
                          public PageSourceWithDelegates {
 public:
  explicit TextFileTab(
    const DXResources&,
    KneeboardState*,
    utf8_string_view title,
    const std::filesystem::path& path);
  explicit TextFileTab(
    const DXResources&,
    KneeboardState*,
    utf8_string_view title,
    const nlohmann::json&);
  virtual ~TextFileTab();

  virtual utf8_string GetGlyph() const override;
  virtual utf8_string GetTitle() const override;
  virtual void Reload() override;

  virtual nlohmann::json GetSettings() const override;

  std::filesystem::path GetPath() const;
  virtual void SetPath(const std::filesystem::path& path);

  uint16_t GetPageCount() const override;

 private:
  std::filesystem::path mPath;
  std::filesystem::file_time_type mLastWriteTime;
  std::shared_ptr<PlainTextPageSource> mPageSource;
  winrt::Windows::Storage::Search::StorageFileQueryResult mQueryResult {
    nullptr};

  std::string GetFileContent() const;

  winrt::fire_and_forget SubscribeToChanges() noexcept;
  void OnFileModified() noexcept;
};

}// namespace OpenKneeboard
