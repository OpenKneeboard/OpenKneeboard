#include "GetIconFromExecutable.h"

#include <shellapi.h>

namespace OpenKneeboard {

wxIcon GetIconFromExecutable(const std::filesystem::path& path) {
  auto wpath = path.wstring();
  HICON handle = ExtractIcon(GetModuleHandle(NULL), wpath.c_str(), 0);
  if (!handle) {
    return {};
  }

  wxIcon icon;
  if (icon.CreateFromHICON(handle)) {
    // `icon` now owns the handle
    return icon;
  }

  DestroyIcon(handle);

  return {};
}

}// namespace OpenKneeboard
