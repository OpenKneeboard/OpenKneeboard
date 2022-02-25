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
#include "AboutPage.xaml.h"
#include "AboutPage.g.cpp"
// clang-format on

#include <OpenKneeboard/utf8.h>
#include <OpenKneeboard/version.h>
#include <appmodel.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <time.h>

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

AboutPage::AboutPage() {
  InitializeComponent();

  std::string_view commitID(Version::CommitID);

  const auto version = fmt::format(
    "{}.{}.{}.{}-{}{}{}{}",
    Version::Major,
    Version::Minor,
    Version::Patch,
    Version::Build,
    commitID.substr(commitID.length() - 6),
    Version::HaveModifiedFiles ? "-dirty" : "",
    Version::IsGithubActionsBuild ? "" : "-notCI",
#ifdef DEBUG
    "-debug"
#else
    ""
#endif
  );

  tm commitTime;
  gmtime_s(&commitTime, &Version::CommitUnixTimestamp);

  UINT32 packageNameLen = MAX_PATH;
  wchar_t packageName[MAX_PATH];
  if (
    GetCurrentPackageFullName(&packageNameLen, packageName) != ERROR_SUCCESS) {
    packageNameLen = 0;
  }

  auto details = fmt::format(
    "Version {}\n\n"
    "Copyright (C) 2021-2022 Fred Emmott.\n\n"
    "Package: {}\n"
    "Built at: {}\n"
    "Build type: {}-{}\n"
    "Commit at: {:%Y-%m-%dT%H:%M:%SZ}\n"
    "Commit ID: {}\n",
    version,
    packageNameLen ? to_utf8(std::wstring_view(packageName, packageNameLen))
                   : "Unpackaged",
    Version::BuildTimestamp,
    BUILD_CONFIG,
#ifdef _WIN32
#ifdef _WIN64
    "Win64",
#else
    "Win32",
#endif
#endif
    commitTime,
    Version::CommitID);
  if constexpr (Version::HaveModifiedFiles) {
    std::string files = Version::ModifiedFiles;
    details += "\nModified files:\n" + files;
  }
  DetailsText().Text(winrt::to_hstring(details));
}

}// namespace winrt::OpenKneeboardApp::implementation
