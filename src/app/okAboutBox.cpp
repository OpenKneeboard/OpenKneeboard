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
#include "okAboutBox.h"

#include <OpenKneeboard/version.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <shims/wx.h>
#include <wx/aboutdlg.h>

using namespace OpenKneeboard;

void okAboutBox(wxWindow* parent) {
  wxAboutDialogInfo info;
  info.SetName("OpenKneeboard");
  info.SetCopyright("(C) 2021-2022 Fred Emmott");

  std::string_view commitID(Version::CommitID);

  info.SetVersion(fmt::format(
    "v{}.{}.{}-{}{}{}",
    Version::Major,
    Version::Minor,
    Version::Patch,
    commitID.substr(commitID.length() - 6),
    Version::HaveModifiedFiles ? "-dirty" : "",
#ifdef DEBUG
    "-debug"
#else
    ""
#endif
    ));

  auto commitTime = *std::gmtime(&Version::CommitUnixTimestamp);

  std::string description = fmt::format(
    "An open source kneeboard.\n\n"
    "Built at: {}\n"
    "Build type: {}-{}\n"
    "Commit at: {:%Y-%m-%dT%H:%M:%SZ}\n"
    "Commit ID: {}\n",
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
    description += "\nModified files:\n" + files;
  }
  info.SetDescription(description);

  wxAboutBox(info, parent);
}
