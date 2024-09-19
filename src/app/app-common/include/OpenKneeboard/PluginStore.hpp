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