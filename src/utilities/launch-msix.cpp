/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or * modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; version 2.
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

#include <Windows.h>
#include <winrt/base.h>
#include <winrt/windows.applicationmodel.core.h>
#include <winrt/windows.applicationmodel.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.management.deployment.h>

// We only need a standard `main()` function, but using wWinMain prevents
// a window/task bar entry from temporarily appearing
int WINAPI wWinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  PWSTR pCmdLine,
  int nCmdShow) {
  // Find with PowerShell: `Get-AppxPackage *OpenKneeboard*`
  constexpr auto packageFamilyName
    = L"FredEmmott.Self.OpenKneeboard_qvw5xrmsm8j1t";

  winrt::Windows::Management::Deployment::PackageManager pm;
  for (const auto& package: pm.FindPackagesForUser({})) {
    if (package.Id().FamilyName() != packageFamilyName) {
      continue;
    }
    auto apps = package.GetAppListEntries();
    apps.GetAt(0).LaunchAsync().get();
    return 0;
  }
  return 1;
}
