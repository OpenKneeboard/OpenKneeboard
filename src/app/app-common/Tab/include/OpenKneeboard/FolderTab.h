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

#include <OpenKneeboard/ITabWithSettings.h>
#include <OpenKneeboard/PageSourceWithDelegates.h>
#include <OpenKneeboard/TabBase.h>

#include <shims/filesystem>

namespace OpenKneeboard {

class FolderPageSource;

class FolderTab final : public TabBase,
                        public PageSourceWithDelegates,
                        public ITabWithSettings {
 public:
  explicit FolderTab(
    const DXResources&,
    KneeboardState*,
    const std::filesystem::path& path);
  explicit FolderTab(
    const DXResources&,
    KneeboardState*,
    const winrt::guid& persistentID,
    utf8_string_view title,
    const nlohmann::json&);
  virtual ~FolderTab();
  virtual utf8_string GetGlyph() const override;

  virtual nlohmann::json GetSettings() const final override;

  virtual void Reload() final override;

  std::filesystem::path GetPath() const;
  virtual void SetPath(const std::filesystem::path& path);

 private:
  FolderTab(
    const DXResources&,
    KneeboardState*,
    const winrt::guid& persistentID,
    utf8_string_view title,
    const std::filesystem::path& path);

  std::shared_ptr<FolderPageSource> mPageSource;
  std::filesystem::path mPath;
};

}// namespace OpenKneeboard
