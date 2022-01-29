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

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <OpenKneeboard/version.h>
#include <fmt/format.h>
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

  std::string description = fmt::format(
    "An open source kneeboard.\n\n"
    "Built at: {}\n"
    "Commit ID: {}",
    Version::BuildTimestamp,
    Version::CommitID);
  if constexpr (Version::HaveModifiedFiles) {
    std::string files = Version::ModifiedFiles;
    description += "\nModified files:\n" + files;
  }
  info.SetDescription(description);

  wxAboutBox(info, parent);
}
