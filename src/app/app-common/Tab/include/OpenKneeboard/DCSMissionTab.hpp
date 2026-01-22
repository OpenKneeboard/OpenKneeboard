// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "DCSTab.hpp"
#include "TabBase.hpp"

#include <OpenKneeboard/IHasDebugInformation.hpp>
#include <OpenKneeboard/PageSourceWithDelegates.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

namespace OpenKneeboard {

struct DXResources;
class DCSExtractedMission;

class DCSMissionTab final : public TabBase,
                            public DCSTab,
                            public PageSourceWithDelegates,
                            public IHasDebugInformation {
 public:
  DCSMissionTab(const audited_ptr<DXResources>&, KneeboardState*);
  DCSMissionTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title);
  virtual ~DCSMissionTab();
  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  [[nodiscard]]
  virtual task<void> Reload() override;

  virtual std::string GetDebugInformation() const override;

 protected:
  virtual OpenKneeboard::fire_and_forget
    OnAPIEvent(APIEvent, std::filesystem::path, std::filesystem::path) override;

 private:
  audited_ptr<DXResources> mDXR {};
  KneeboardState* mKneeboard {nullptr};
  std::filesystem::path mMission;
  std::string mAircraft;
  std::shared_ptr<DCSExtractedMission> mExtracted;
  std::string mDebugInformation;
};

}// namespace OpenKneeboard
