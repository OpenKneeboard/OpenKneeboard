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
  virtual void RenderPage(
    uint16_t pageIndex,
    const winrt::com_ptr<ID2D1RenderTarget>& target,
    const D2D1_RECT_F& rect) final override;
  virtual D2D1_SIZE_U GetPreferredPixelSize(uint16_t pageIndex) final override;

  std::filesystem::path GetPath() const;
  virtual void SetPath(const std::filesystem::path& path);

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};
}// namespace OpenKneeboard
