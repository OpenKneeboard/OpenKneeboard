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

#include <OpenKneeboard/ITabWithSettings.hpp>
#include <OpenKneeboard/KneeboardViewID.hpp>
#include <OpenKneeboard/PageSourceWithDelegates.hpp>
#include <OpenKneeboard/Plugin.hpp>
#include <OpenKneeboard/TabBase.hpp>

#include <OpenKneeboard/json.hpp>

#include <optional>

namespace OpenKneeboard {

class PluginTab final : public TabBase,
                        public ITabWithSettings,
                        public PageSourceWithDelegates {
 public:
  struct Settings {
    std::string mPluginTabTypeID;

    bool operator==(const Settings&) const noexcept = default;
  };

  PluginTab() = delete;
  static task<std::shared_ptr<PluginTab>> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const Settings&);

  ~PluginTab();

  std::string GetGlyph() const override;
  task<void> Reload() final override;
  nlohmann::json GetSettings() const override;

  std::string GetPluginTabTypeID() const noexcept;

  void PostCustomAction(
    KneeboardViewID,
    std::string_view id,
    const nlohmann::json& arg);

 private:
  PluginTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const Settings&);
  audited_ptr<DXResources> mDXResources;
  KneeboardState* mKneeboard {nullptr};
  Settings mSettings;

  std::shared_ptr<IPageSource> mDelegate;

  std::optional<Plugin> mPlugin;
  std::optional<Plugin::TabType> mTabType;
};

OPENKNEEBOARD_DECLARE_SPARSE_JSON(PluginTab::Settings);

}// namespace OpenKneeboard