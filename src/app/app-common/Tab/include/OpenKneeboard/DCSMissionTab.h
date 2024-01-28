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
#include "TabBase.h"

#include <OpenKneeboard/IHasDebugInformation.h>
#include <OpenKneeboard/PageSourceWithDelegates.h>

namespace OpenKneeboard {

struct DXResources;
class DCSExtractedMission;

class DCSMissionTab final : public TabBase,
                            public DCSTab,
                            public PageSourceWithDelegates,
                            public IHasDebugInformation {
 public:
  DCSMissionTab(const std::shared_ptr<DXResources>&, KneeboardState*);
  DCSMissionTab(
    const std::shared_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title);
  virtual ~DCSMissionTab();
  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  virtual void Reload() override;

  virtual std::string GetDebugInformation() const override;

 protected:
  virtual void OnGameEvent(
    const GameEvent&,
    const std::filesystem::path&,
    const std::filesystem::path&) override;

 private:
  std::shared_ptr<DXResources> mDXR {};
  KneeboardState* mKneeboard {nullptr};
  std::filesystem::path mMission;
  std::string mAircraft;
  std::shared_ptr<DCSExtractedMission> mExtracted;
  std::string mDebugInformation;
};

}// namespace OpenKneeboard
