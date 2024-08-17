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
#include <OpenKneeboard/LaunchURI.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

#include <unordered_map>

#include <shellapi.h>

static std::unordered_map<std::wstring, std::function<void(std::string_view)>>
  gHandlers;

namespace OpenKneeboard {

void RegisterURIHandler(
  const std::string& schemeName,
  std::function<void(std::string_view)> handler) {
  gHandlers.insert_or_assign(
    std::wstring(winrt::to_hstring(schemeName)), handler);
}

task<void> LaunchURI(std::string_view uriStr) {
  auto uri = winrt::Windows::Foundation::Uri(winrt::to_hstring(uriStr));
  const std::wstring scheme(uri.SchemeName());
  if (gHandlers.contains(scheme)) {
    gHandlers.at(scheme)(uriStr);
    co_return;
  }

  if (scheme == L"https" || scheme == L"http") {
    co_await winrt::Windows::System::Launcher::LaunchUriAsync(uri);
    co_return;
  }

  std::wstring uriWstr(winrt::to_hstring(uriStr));
  ShellExecuteW(NULL, L"open", uriWstr.c_str(), nullptr, nullptr, SW_NORMAL);
}

}// namespace OpenKneeboard
