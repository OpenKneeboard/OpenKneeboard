// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include <OpenKneeboard/Elevation.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/PluginStore.hpp>
#include <OpenKneeboard/Registry.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/json.hpp>

#include <wil/registry.h>

#include <algorithm>
#include <expected>
#include <format>
#include <fstream>
#include <ranges>

namespace OpenKneeboard {
PluginStore::PluginStore() {
  if (OpenKneeboard::IsElevated() || OpenKneeboard::IsShellElevated()) {
    dprint.Warning("not loading any plugins because OpenKneeboard is elevated");
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
      dprint.Error(
        L"Registry value for plugin `{}` is not a DWORD", value.name);
      continue;
    }
    const auto enabled = wil::reg::get_value_dword(hkey, value.name.c_str());
    switch (enabled) {
      case 0:
        dprint(L"Skipping plugin `{}` - disabled in registry", value.name);
        continue;
      case 1:
        dprint(L"Loading plugin `{}` from registry...", value.name);
        if (!std::filesystem::exists(value.name)) {
          dprint("... ERROR: file does not exist.");
          continue;
        }
        this->TryAppend(value.name);
        continue;
      default:
        dprint.Warning(
          L"skipping plugin `{}` from registry - invalid value {}",
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
  dprint("Loading plugin from `{}`", jsonPath);
  try {
    auto plugin
      = nlohmann::json::parse(std::ifstream {jsonPath, std::ios::binary})
          .get<Plugin>();
    plugin.mJSONPath = jsonPath;
    dprint(
      "Parsed plugin ID `{}` (`{}`), version `{}`",
      plugin.mID,
      plugin.mMetadata.mPluginName,
      plugin.mMetadata.mPluginReadableVersion);
    this->Append(plugin);
  } catch (const nlohmann::json::exception& e) {
    dprint.Error("Error {} when parsing plugin: {}", e.id, e.what());
  } catch (const std::exception& e) {
    dprint.Error("C++ exception loading plugin: {}", e.what());
  }
}

std::vector<Plugin> PluginStore::GetPlugins() const noexcept {
  return mPlugins;
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
  dprint(
    "🧩 Loaded plugin '{}' ('{}') version '{}' from `{}`",
    plugin.mMetadata.mPluginName,
    plugin.mID,
    plugin.mMetadata.mPluginReadableVersion,
    plugin.mJSONPath);
}
}// namespace OpenKneeboard