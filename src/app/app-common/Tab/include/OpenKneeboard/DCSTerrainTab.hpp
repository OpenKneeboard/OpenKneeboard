// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "DCSTab.hpp"
#include "TabBase.hpp"

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/IHasDebugInformation.hpp>
#include <OpenKneeboard/PageSourceWithDelegates.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

namespace OpenKneeboard {

class FolderTab;

class DCSTerrainTab final : public TabBase,
                            public DCSTab,
                            public PageSourceWithDelegates,
                            public IHasDebugInformation {
 public:
  DCSTerrainTab(const audited_ptr<DXResources>&, KneeboardState*);
  DCSTerrainTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title);
  ~DCSTerrainTab();

  [[nodiscard]]
  virtual task<void> Reload() override;

  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  virtual std::string GetDebugInformation() const override;

 protected:
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard = nullptr;

  std::string mDebugInformation;

  std::string mTerrain;
  std::vector<std::filesystem::path> mPaths;

  virtual OpenKneeboard::fire_and_forget
    OnAPIEvent(APIEvent, std::filesystem::path, std::filesystem::path) override;
};

}// namespace OpenKneeboard
