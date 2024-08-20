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

#include <OpenKneeboard/Elevation.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/PluginStore.hpp>
#include <OpenKneeboard/Registry.hpp>

#include <wil/registry.h>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/json.hpp>

#include <algorithm>
#include <expected>
#include <format>
#include <fstream>
#include <ranges>

namespace OpenKneeboard {

PluginStore::PluginStore() {
  if (OpenKneeboard::IsElevated() || OpenKneeboard::IsShellElevated()) {
    dprint(
      "WARNING: not loading any plugins because OpenKneeboard is elevated");
    return;
  }
  this->LoadPluginsFromFilesystem();
  this->LoadPluginsFromRegistry();
}

void PluginStore::LoadPluginsFromRegistry() {
  this->LoadPluginsFromRegistry(HKEY_LOCAL_MACHINE);
  this->LoadPluginsFromRegistry(HKEY_CURRENT_USER);
}

void PluginStore::LoadPluginsFromRegistry(HKEY root) {
  const auto subkeyPath
    = std::format(L"{}\\Plugins\\v1", Config::RegistrySubKey);
  const auto expected = Registry::open_unique_key(root, subkeyPath.c_str());
  if (!expected) {
    return;
  }
  const auto hkey = expected.value().get();

  using VIT = wil::reg::value_iterator;
  for (const auto& value: wil::make_range(VIT {hkey}, VIT {})) {
    if (value.type != REG_DWORD) {
      dprintf(
        L"ERROR: Registry value for plugin `{}` is not a DWORD", value.name);
      continue;
    }
    const auto enabled = wil::reg::get_value_dword(hkey, value.name.c_str());
    switch (enabled) {
      case 0:
        dprintf(L"Skipping plugin `{}` - disabled in registry", value.name);
        continue;
      case 1:
        dprintf(L"Loading plugin `{}` from registry...", value.name);
        if (!std::filesystem::exists(value.name)) {
          dprint("... ERROR: file does not exist.");
          continue;
        }
        this->TryAppend(value.name);
        continue;
      default:
        dprintf(
          L"WARNING: skipping plugin `{}` from registry - invalid value {}",
          value.name,
          enabled);
        continue;
    }
  }
}

void PluginStore::LoadPluginsFromFilesystem() {
  const auto root = Filesystem::GetInstalledPluginsDirectory();
  for (const auto& entry: std::filesystem::directory_iterator {root}) {
    if (!entry.is_directory()) {
      continue;
    }
    const auto jsonPath = entry.path() / "v1.json";
    if (!std::filesystem::exists(jsonPath)) {
      continue;
    }
    this->TryAppend(jsonPath);
  }
}

PluginStore::~PluginStore() {
}

void PluginStore::TryAppend(const std::filesystem::path& jsonPath) {
  dprintf("Loading plugin from `{}`", jsonPath);
  try {
    const auto plugin
      = nlohmann::json::parse(std::ifstream {jsonPath, std::ios::binary})
          .get<Plugin>();
    dprintf(
      "Parsed plugin ID `{}` (`{}`), version `{}`",
      plugin.mID,
      plugin.mMetadata.mPluginName,
      plugin.mMetadata.mPluginReadableVersion);
    this->Append(plugin);
  } catch (const nlohmann::json::exception& e) {
    dprintf("ERROR: error {} when parsing plugin: {}", e.id, e.what());
    OPENKNEEBOARD_BREAK;
  } catch (const std::exception& e) {
    dprintf("ERROR: C++ exception loading plugin: {}", e.what());
    OPENKNEEBOARD_BREAK;
  }
}

std::vector<Plugin::TabType> PluginStore::GetTabTypes() const noexcept {
  return mPlugins | std::views::transform(&Plugin::mTabTypes) | std::views::join
    | std::ranges::to<std::vector>();
}

void PluginStore::Append(const Plugin& plugin) {
  const auto it = std::ranges::find(mPlugins, plugin.mID, &Plugin::mID);
  if (it != mPlugins.end()) {
    mPlugins.erase(it);
  }
  mPlugins.push_back(plugin);
}

}// namespace OpenKneeboard