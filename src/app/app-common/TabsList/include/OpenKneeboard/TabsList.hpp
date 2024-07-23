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

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/ITab.hpp>

#include <shims/nlohmann/json_fwd.hpp>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/inttypes.hpp>

namespace OpenKneeboard {
class KneeboardState;

class TabsList final : private EventReceiver {
 public:
  TabsList() = delete;
  TabsList(
    const audited_ptr<DXResources>&,
    KneeboardState* kneeboard,
    const nlohmann::json& config);
  ~TabsList();

  std::vector<std::shared_ptr<ITab>> GetTabs() const;

  [[nodiscard]] IAsyncAction RemoveTab(TabIndex index);
  [[nodiscard]] IAsyncAction InsertTab(
    TabIndex index,
    std::shared_ptr<ITab> tab);
  [[nodiscard]]
  IAsyncAction SetTabs(std::vector<std::shared_ptr<ITab>> tabs);

  nlohmann::json GetSettings() const;
  void LoadSettings(const nlohmann::json&);

  Event<> evSettingsChangedEvent;
  Event<std::vector<std::shared_ptr<ITab>>> evTabsChangedEvent;

 private:
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard;
  std::vector<std::shared_ptr<ITab>> mTabs;
  std::vector<EventHandlerToken> mTabEvents;

  void LoadDefaultSettings();
};

}// namespace OpenKneeboard
