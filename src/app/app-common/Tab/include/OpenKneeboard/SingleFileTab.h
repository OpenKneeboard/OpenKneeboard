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

#include "ITab.h"
#include "ITabWithSettings.h"
#include "TabBase.h"

#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/PageSourceWithDelegates.h>

#include <OpenKneeboard/audited_ptr.h>

#include <shims/filesystem>

namespace OpenKneeboard {

class PlainTextFilePageSource;

class SingleFileTab final : public TabBase,
                            public ITabWithSettings,
                            public PageSourceWithDelegates {
 public:
  enum class Kind {
    Unknown,
    PDFFile,
    PlainTextFile,
    ImageFile,
  };
  SingleFileTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const std::filesystem::path& path);
  SingleFileTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const nlohmann::json&);
  virtual ~SingleFileTab();

  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();
  [[nodiscard]]
  virtual winrt::Windows::Foundation::IAsyncAction Reload() override;

  virtual nlohmann::json GetSettings() const override;

  std::filesystem::path GetPath() const;
  [[nodiscard]]
  virtual winrt::Windows::Foundation::IAsyncAction SetPath(
    std::filesystem::path);

 private:
  SingleFileTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const std::filesystem::path& path);
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard;

  void SetPathFromEmpty(const std::filesystem::path& path);

  Kind mKind = Kind::Unknown;
  std::filesystem::path mPath;
};

}// namespace OpenKneeboard
