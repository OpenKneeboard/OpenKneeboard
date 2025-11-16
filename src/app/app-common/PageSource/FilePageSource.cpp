// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/ChromiumPageSource.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/FilePageSource.hpp>
#include <OpenKneeboard/ImageFilePageSource.hpp>
#include <OpenKneeboard/PDFFilePageSource.hpp>
#include <OpenKneeboard/PlainTextFilePageSource.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/format/filesystem.hpp>

#include <shims/winrt/base.h>

#include <algorithm>
#include <ranges>

#include <icu.h>

namespace OpenKneeboard {
std::vector<std::string> FilePageSource::GetSupportedExtensions(
  const audited_ptr<DXResources>& dxr) noexcept {
  std::vector<std::string> ret {".txt", ".pdf", ".htm", ".html"};

  auto images = ImageFilePageSource::GetFileFormatProviders(dxr->mWIC.get())
    | std::views::transform(
                  &ImageFilePageSource::FileFormatProvider::mExtensions)
    | std::views::join;

  std::ranges::unique_copy(images, std::back_inserter(ret));

  return ret;
}

task<std::shared_ptr<IPageSource>> FilePageSource::Create(
  audited_ptr<DXResources> dxr,
  KneeboardState* kbs,
  std::filesystem::path path) noexcept {
  try {
    if (!std::filesystem::is_regular_file(path)) {
      dprint("FilePageSource file '{}' is not a regular file", path);
      co_return nullptr;
    }
  } catch (const std::filesystem::filesystem_error& e) {
    dprint(
      "FilePageSource failed to get status of file '{}': {} ({})",
      path,
      e.what(),
      e.code().value());
    co_return nullptr;
  }

  const auto extension = path.extension().u16string();

  const auto hasExtension = [a = extension.c_str()](auto b) {
    return u_strcasecmp(a, b, U_FOLD_CASE_DEFAULT) == 0;
  };

  if (hasExtension(u".pdf")) {
    co_return co_await PDFFilePageSource::Create(dxr, kbs, path);
  }

  if (hasExtension(u".txt")) {
    co_return co_await PlainTextFilePageSource::Create(dxr, kbs, path);
  }

  if (hasExtension(u".htm") || hasExtension(u".html")) {
    co_return co_await ChromiumPageSource::Create(dxr, kbs, path);
  }

  if (ImageFilePageSource::CanOpenFile(dxr, path)) {
    co_return ImageFilePageSource::Create(
      dxr, std::vector<std::filesystem::path> {path});
  }

  dprint("Couldn't find handler for {}", path);

  co_return nullptr;
}
}// namespace OpenKneeboard