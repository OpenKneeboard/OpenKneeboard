#pragma once

#include <filesystem>

#include "OpenKneeboard/Tab.h"

namespace OpenKneeboard {

class FolderTab : public Tab {
 public:
  FolderTab(const std::string& title, const std::filesystem::path& path);
  virtual ~FolderTab();

  virtual void Reload() final override;
  virtual uint16_t GetPageCount() const final override;
  virtual wxImage RenderPage(uint16_t) final override;

 protected:
  void SetPath(const std::filesystem::path& path);

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};
}// namespace OpenKneeboard
