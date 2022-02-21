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
#include <OpenKneeboard/TextFileTab.h>
#include <ShlObj.h>
#include <shims/wx.h>
#include <wx/filedlg.h>

#include <nlohmann/json.hpp>

namespace OpenKneeboard {

TextFileTab::TextFileTab(
  const DXResources& dxr,
  utf8_string_view title,
  const nlohmann::json& settings)
  : TextFileTab(
    dxr,
    title,
    std::filesystem::path(settings.at("Path").get<std::string>())) {
}

std::shared_ptr<TextFileTab> TextFileTab::Create(
  wxWindow* parent,
  const DXResources& dxr) {
  wchar_t* documentsDir = nullptr;
  SHGetKnownFolderPath(FOLDERID_Documents, NULL, NULL, &documentsDir);

  wxFileDialog dialog(
    parent,
    _("Add Text File Tab"),
    documentsDir,
    {},
    _("UTF-8 Text (*.txt)|*.txt"),
    wxFD_DEFAULT_STYLE | wxFD_OPEN | wxFD_FILE_MUST_EXIST);

  if (dialog.ShowModal() == wxID_CANCEL) {
    return nullptr;
  }

  std::filesystem::path path(dialog.GetPath().ToStdWstring());
  if (!std::filesystem::is_regular_file(path)) {
    return nullptr;
  }

  return std::make_shared<TextFileTab>(dxr, path.stem(), path);
}

nlohmann::json TextFileTab::GetSettings() const {
  return {{"Path", to_utf8(GetPath())}};
}

}// namespace OpenKneeboard
