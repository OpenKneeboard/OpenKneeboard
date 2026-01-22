// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/ITab.hpp>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/inttypes.hpp>
#include <OpenKneeboard/task.hpp>

#include <shims/nlohmann/json_fwd.hpp>

namespace OpenKneeboard {
class KneeboardState;

class TabsList final : private EventReceiver {
 public:
  TabsList() = delete;
  ~TabsList();

  static task<std::shared_ptr<TabsList>> Create(
    const audited_ptr<DXResources>&,
    KneeboardState* kneeboard,
    const nlohmann::json& config);

  std::vector<std::shared_ptr<ITab>> GetTabs() const;

  [[nodiscard]] task<void> RemoveTab(TabIndex index);
  [[nodiscard]] task<void> InsertTab(TabIndex index, std::shared_ptr<ITab> tab);
  [[nodiscard]]
  task<void> SetTabs(std::vector<std::shared_ptr<ITab>> tabs);

  nlohmann::json GetSettings() const;
  [[nodiscard]]
  task<void> LoadSettings(nlohmann::json);

  Event<> evSettingsChangedEvent;
  Event<> evTabsChangedEvent;

 private:
  TabsList(const audited_ptr<DXResources>&, KneeboardState* kneeboard);

  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard;
  std::vector<std::shared_ptr<ITab>> mTabs;
  std::vector<EventHandlerToken> mTabEvents;

  task<std::shared_ptr<ITab>> LoadTabFromJSON(nlohmann::json tab);

  [[nodiscard]]
  task<void> LoadDefaultSettings();
};

}// namespace OpenKneeboard
