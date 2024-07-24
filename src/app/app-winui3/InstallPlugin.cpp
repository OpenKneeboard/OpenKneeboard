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

// clang-format off
#include "pch.h"
#include "InstallPlugin.h"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/Filesystem.hpp>

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <wil/resource.h>

#include <processenv.h>
#include <shellapi.h>

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt;

namespace OpenKneeboard {

static IAsyncAction InstallPlugin(
  XamlRoot& xamlRoot,
  std::filesystem::path path) {
  MessageBoxW(
    NULL,
    std::format(L"Asked to install plugin from `{}`", path).c_str(),
    L"OpenKneeboard",
    MB_OK | MB_ICONINFORMATION);
  co_return;
}

IAsyncAction InstallPlugin(
  XamlRoot xamlRoot,
  const wchar_t* const commandLine) {
  int argc {};
  const wil::unique_hlocal_ptr<PWSTR[]> argv {
    CommandLineToArgvW(commandLine, &argc)};

  for (int i = 0; i < argc; ++i) {
    const std::wstring_view arg {argv[i]};
    if (arg != L"--plugin") {
      continue;
    }
    if (i == argc - 1) {
      dprint("ERROR: `--plugin` passed, but no plugin specified.");
      OPENKNEEBOARD_BREAK;
      co_return;
    }
    co_await InstallPlugin(xamlRoot, std::filesystem::path {argv[i + 1]});
    co_return;
  }

  co_return;
}
}// namespace OpenKneeboard
