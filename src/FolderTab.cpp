#include "YAVRK/FolderTab.h"

namespace YAVRK {
class FolderTab::Impl final {
 public:
  std::filesystem::path Path;
  std::vector<wxImage> Pages = {};
};

FolderTab::FolderTab(
  const std::string& title,
  const std::filesystem::path& path)
  : Tab(title), p(new Impl {.Path = path}) {
  Reload();
}

FolderTab::~FolderTab() {
}

void FolderTab::Reload() {
  p->Pages.clear();
  if (!std::filesystem::is_directory(p->Path)) {
    return;
  }
  for (auto& entry : std::filesystem::recursive_directory_iterator(p->Path)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    auto wsPath = entry.path().wstring();
    if (!wxImage::CanRead(wsPath)) {
      continue;
    }
    wxImage page(wsPath);
    if (!page.IsOk()) {
      continue;
    }

    p->Pages.push_back(page);
  }
}

uint16_t FolderTab::GetPageCount() const {
  return p->Pages.size();
}

wxImage FolderTab::RenderPage(uint16_t index) {
  if (index >= GetPageCount()) {
    return wxImage();
  }
  return p->Pages.at(index);
}

void FolderTab::SetPath(const std::filesystem::path& path) {
  p->Path = path;
  Reload();
}

}// namespace YAVRK
