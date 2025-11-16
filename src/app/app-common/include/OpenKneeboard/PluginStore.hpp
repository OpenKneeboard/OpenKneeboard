// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/Plugin.hpp>

namespace OpenKneeboard {

class PluginStore final {
 public:
  PluginStore();
  ~PluginStore();

  std::vector<Plugin> GetPlugins() const noexcept;
  std::vector<Plugin::TabType> GetTabTypes() const noexcept;
  void Append(const Plugin&);

  Event<> evTabTypesChanged;

 private:
  std::vector<Plugin> mPlugins;

  void LoadPluginsFromFilesystem();
  void LoadPluginsFromRegistry();
  void LoadPluginsFromRegistry(HKEY root);

  void TryAppend(const std::filesystem::path&);
};

}// namespace OpenKneeboard