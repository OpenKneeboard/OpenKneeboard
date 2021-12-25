#include "YAVRK/FolderTab.h"

namespace YAVRK {
class FolderTab::Impl final {
 public:
  std::filesystem::path Path;
  std::vector<wxImage> Pages = {};
  std::vector<std::filesystem::path> PagePaths = {};
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
  p->PagePaths.clear();
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
    p->PagePaths.push_back(wsPath);
  }
  p->Pages.resize(p->PagePaths.size());
}

uint16_t FolderTab::GetPageCount() const {
  return p->Pages.size();
}

wxImage FolderTab::RenderPage(uint16_t index) {
  if (index >= GetPageCount()) {
    return wxImage();
  }

  auto image = p->Pages.at(index);
  if (image.IsOk()) {
    return image;
  }

  image.LoadFile(p->PagePaths.at(index).wstring());
  if (image.IsOk()) {
    p->Pages[index] = image;
    return image;
  }

  p->Pages.erase(p->Pages.begin() + index);
  p->PagePaths.erase(p->PagePaths.begin() + index);

  return RenderPage(index);
}

void FolderTab::SetPath(const std::filesystem::path& path) {
  p->Path = path;
  Reload();
}

}// namespace YAVRK
