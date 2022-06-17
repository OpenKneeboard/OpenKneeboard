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
#include "CheckRuntimeFiles.h"
// clang-format on

#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/utf8.h>
#include <microsoft.ui.xaml.window.h>
#include <shobjidl.h>
#include <winrt/microsoft.ui.xaml.controls.h>

#include <filesystem>
#include <format>

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt;

namespace OpenKneeboard {

winrt::Windows::Foundation::IAsyncAction CheckRuntimeFiles(
  const XamlRoot& root) {
  do {
    std::string message;
    try {
      RuntimeFiles::Install();
      co_return;
    } catch (const std::filesystem::filesystem_error& error) {
      message = std::format(
        _("OpenKneeboard couldn't update the helper files, so "
          "might not work correctly; close any games that "
          "you use with OpenKneeboard, and try "
          "again.\n\nError {:#x}: {}\n{}"),
        error.code().value(),
        error.code().message(),
        error.what());
    }

    ContentDialog dialog;
    dialog.XamlRoot(root);
    dialog.Title(
      winrt::box_value(winrt::to_hstring(_("Can't Update Helper Files"))));
    dialog.DefaultButton(ContentDialogButton::Primary);
    dialog.PrimaryButtonText(winrt::to_hstring(_("Retry")));
    dialog.CloseButtonText(winrt::to_hstring(_("Ignore")));
    dialog.Content(winrt::box_value(winrt::to_hstring(message)));
    if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
      co_return;
    }
  } while (true);
}

}// namespace OpenKneeboard
