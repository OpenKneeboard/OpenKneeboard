// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/ITabWithSettings.hpp>
#include <OpenKneeboard/PageSourceWithDelegates.hpp>
#include <OpenKneeboard/TabBase.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

#include <filesystem>

namespace OpenKneeboard {

class FolderPageSource;

class FolderTab final : public TabBase,
                        public PageSourceWithDelegates,
                        public ITabWithSettings {
 public:
  static task<std::shared_ptr<FolderTab>> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const std::filesystem::path& path);
  static task<std::shared_ptr<FolderTab>> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const nlohmann::json&);
  virtual ~FolderTab();
  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  virtual nlohmann::json GetSettings() const final override;

  [[nodiscard]]
  virtual task<void> Reload() final override;

  std::filesystem::path GetPath() const;
  [[nodiscard]]
  virtual task<void> SetPath(std::filesystem::path);

 private:
  FolderTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title);

  audited_ptr<DXResources> mDXResources;
  KneeboardState* mKneeboard {nullptr};

  std::shared_ptr<FolderPageSource> mPageSource;
  std::filesystem::path mPath;
};

}// namespace OpenKneeboard
