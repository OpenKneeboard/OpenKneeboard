#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <ShlObj.h>
#include <wx/dirdlg.h>

#include "OpenKneeboard/FolderTab.h"

namespace OpenKneeboard {

std::shared_ptr<FolderTab> FolderTab::FromSettings(
  const std::string& title,
  const nlohmann::json& settings) {
  return std::make_shared<FolderTab>(title, settings.at("Path"));
}

std::shared_ptr<FolderTab> FolderTab::Create(wxWindow* parent) {
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

  return std::make_shared<FolderTab>(path.stem().string(), path);
}

nlohmann::json FolderTab::GetSettings() const {
  return {{"Path", GetPath().string()}};
}

}// namespace OpenKneeboard
