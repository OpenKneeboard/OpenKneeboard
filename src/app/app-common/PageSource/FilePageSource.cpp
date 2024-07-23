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
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/FilePageSource.hpp>
#include <OpenKneeboard/ImageFilePageSource.hpp>
#include <OpenKneeboard/PDFFilePageSource.hpp>
#include <OpenKneeboard/PlainTextFilePageSource.hpp>
#include <OpenKneeboard/WebView2PageSource.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <ranges>

#include <icu.h>

namespace OpenKneeboard {

std::vector<std::string> FilePageSource::GetSupportedExtensions(
  const audited_ptr<DXResources>& dxr) noexcept {
  std::vector<std::string> extensions {".txt", ".pdf", ".htm", ".html"};

  winrt::com_ptr<IEnumUnknown> enumerator;
  winrt::check_hresult(
    dxr->mWIC->CreateComponentEnumerator(WICDecoder, 0, enumerator.put()));

  winrt::com_ptr<IUnknown> it;
  ULONG fetched {};
  while (enumerator->Next(1, it.put(), &fetched) == S_OK) {
    const scope_exit clearIt([&]() { it = {nullptr}; });
    const auto info = it.as<IWICBitmapCodecInfo>();
    UINT neededBufferSize = 0;
    winrt::check_hresult(
      info->GetFileExtensions(0, nullptr, &neededBufferSize));
    std::wstring buffer(neededBufferSize, 0);
    winrt::check_hresult(info->GetFileExtensions(
      neededBufferSize, buffer.data(), &neededBufferSize));
    buffer.pop_back();// remove trailing null

    for (const auto& range: std::ranges::views::split(buffer, L',')) {
      extensions.push_back(
        winrt::to_string(std::wstring_view(range.begin(), range.end())));
    }
  }

  return extensions;
}

std::shared_ptr<IPageSource> FilePageSource::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path) noexcept {
  try {
    if (!std::filesystem::is_regular_file(path)) {
      dprintf("FilePageSource file '{}' is not a regular file", path);
      return {nullptr};
    }
  } catch (const std::filesystem::filesystem_error& e) {
    dprintf(
      "FilePageSource failed to get status of file '{}': {} ({})",
      path,
      e.what(),
      e.code().value());
    return {nullptr};
  }

  const auto extension = path.extension().u16string();

  const auto hasExtension = [a = extension.c_str()](auto b) {
    return u_strcasecmp(a, b, U_FOLD_CASE_DEFAULT) == 0;
  };

  if (hasExtension(u".pdf")) {
    return PDFFilePageSource::Create(dxr, kbs, path);
  }

  if (hasExtension(u".txt")) {
    return PlainTextFilePageSource::Create(dxr, kbs, path);
  }

  if (hasExtension(u".htm") || hasExtension(u".html")) {
    return WebView2PageSource::Create(dxr, kbs, path);
  }

  if (ImageFilePageSource::CanOpenFile(dxr, path)) {
    return ImageFilePageSource::Create(
      dxr, std::vector<std::filesystem::path> {path});
  }

  dprintf("Couldn't find handler for {}", path);

  return {nullptr};
}

}// namespace OpenKneeboard
