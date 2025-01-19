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
#include <Windows.h>
#include <winrt/base.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Management.Deployment.h>
// clang-format on

int wWinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  PWSTR pCmdLine,
  int nCmdShow) {
  // *Very* old versions (2022 and earlier) of Openkneeboard used MSIX
  // installers instead of MSI, so aren't automatically removed when
  // newer versions are installed.
  //
  // Remove them here :)
  winrt::init_apartment();
  winrt::Windows::Management::Deployment::PackageManager pm;
  for (auto&& package: pm.FindPackages()) {
    auto id = package.Id();
    // We don't check the full name, as that is derived from the signing
    // public key; releases were signed with multiple keys.
    if (!id.FamilyName().starts_with(L"FredEmmott.Self.OpenKneeboard")) {
      continue;
    }
    using RemovalOptions
      = winrt::Windows::Management::Deployment::RemovalOptions;
    pm.RemovePackageAsync(id.FullName(), RemovalOptions::RemoveForAllUsers);
  }

  return 0;
}