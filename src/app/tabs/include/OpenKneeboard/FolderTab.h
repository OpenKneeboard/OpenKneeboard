#pragma once

#include <filesystem>

#include "OpenKneeboard/Tab.h"

class wxWindow;

namespace OpenKneeboard {

class FolderTab : public Tab {
 public:
  FolderTab(const wxString& title, const std::filesystem::path& path);
  virtual ~FolderTab();

  static std::shared_ptr<FolderTab> FromSettings(
    const std::string& title,
    const nlohmann::json&);
  static std::shared_ptr<FolderTab> Create(wxWindow* parent);

  virtual nlohmann::json GetSettings() const final override;

  virtual void Reload() final override;
  virtual uint16_t GetPageCount() const final override;
  virtual wxImage RenderPage(uint16_t) final override;

  std::filesystem::path GetPath() const;
  virtual void SetPath(const std::filesystem::path& path);

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};
}// namespace OpenKneeboard
