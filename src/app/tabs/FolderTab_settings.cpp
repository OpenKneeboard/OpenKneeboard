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
#include <OpenKneeboard/FolderTab.h>
#include <ShlObj.h>
#include <shims/wx.h>
#include <wx/dirdlg.h>

namespace OpenKneeboard {

FolderTab::FolderTab(
  const DXResources& dxr,
  utf8_string_view title,
  const nlohmann::json& settings)
  : FolderTab(
    dxr,
    title,
    std::filesystem::path(settings.at("Path").get<std::string>())) {
}

std::shared_ptr<FolderTab> FolderTab::Create(
  wxWindow* parent,
  const DXResources& dxr) {
  wxDirDialog dialog(
    parent, _("Add Folder Tab"), {}, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

  wchar_t* userDir = nullptr;
  if (SHGetKnownFolderPath(FOLDERID_Documents, NULL, NULL, &userDir) == S_OK) {
    dialog.SetPath(userDir);
  }

  if (dialog.ShowModal() == wxID_CANCEL) {
    return nullptr;
  }

  std::filesystem::path path(dialog.GetPath().ToStdWstring());
  if (!std::filesystem::is_directory(path)) {
    return nullptr;
  }

  return std::make_shared<FolderTab>(dxr, path.stem(), path);
}

nlohmann::json FolderTab::GetSettings() const {
  return {{"Path", to_utf8(GetPath())}};
}

wxWindow* FolderTab::GetSettingsUI(wxWindow* parent) {
  return nullptr;
}

}// namespace OpenKneeboard
