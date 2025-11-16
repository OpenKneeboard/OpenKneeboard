// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "ITab.hpp"
#include "ITabWithSettings.hpp"
#include "TabBase.hpp"

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/PageSourceWithDelegates.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

#include <filesystem>

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
    HTMLFile,
  };
  static task<std::shared_ptr<SingleFileTab>> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const std::filesystem::path& path);
  static task<std::shared_ptr<SingleFileTab>> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const nlohmann::json&);
  virtual ~SingleFileTab();

  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();
  [[nodiscard]]
  virtual task<void> Reload() override;

  virtual nlohmann::json GetSettings() const override;

  std::filesystem::path GetPath() const;
  [[nodiscard]]
  virtual task<void> SetPath(std::filesystem::path);

 private:
  static task<std::shared_ptr<SingleFileTab>> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const std::filesystem::path& path);
  SingleFileTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title);
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard;

  Kind mKind = Kind::Unknown;
  std::filesystem::path mPath;
};

}// namespace OpenKneeboard
