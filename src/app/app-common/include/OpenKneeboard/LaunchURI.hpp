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

#include <OpenKneeboard/task.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Foundation.h>

#include <functional>
#include <string>

namespace OpenKneeboard {

namespace SpecialURIs {
constexpr std::string_view Scheme {"openkneeboard"};

namespace Paths {
constexpr std::string_view SettingsGames {"Settings/Games"};
constexpr std::string_view SettingsInput {"Settings/Input"};
constexpr std::string_view SettingsTabs {"Settings/Tabs"};
}// namespace Paths

const std::string SettingsInput
  = std::format("{}:///{}", Scheme, Paths::SettingsInput);

};// namespace SpecialURIs

void RegisterURIHandler(
  std::string_view schemeName,
  std::function<void(std::string_view)> handler);

task<void> LaunchURI(std::string_view uri);

}// namespace OpenKneeboard
