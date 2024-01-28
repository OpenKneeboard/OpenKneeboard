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
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/FilePageSource.h>
#include <OpenKneeboard/ImageFilePageSource.h>
#include <OpenKneeboard/PDFFilePageSource.h>
#include <OpenKneeboard/PlainTextFilePageSource.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/utf8.h>

#include <ranges>

#include <icu.h>

namespace OpenKneeboard {

std::vector<std::string> FilePageSource::GetSupportedExtensions(
  const std::shared_ptr<DXResources>& dxr) noexcept {
  std::vector<std::string> extensions {".txt", ".pdf"};

  winrt::com_ptr<IEnumUnknown> enumerator;
  winrt::check_hresult(
    dxr->mWIC->CreateComponentEnumerator(WICDecoder, 0, enumerator.put()));

  winrt::com_ptr<IUnknown> it;
  ULONG fetched;
  while (enumerator->Next(1, it.put(), &fetched) == S_OK) {
    const scope_guard clearIt([&]() { it = {nullptr}; });
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
  const std::shared_ptr<DXResources>& dxr,
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

  if (u_strcasecmp(extension.c_str(), u".pdf", U_FOLD_CASE_DEFAULT) == 0) {
    return PDFFilePageSource::Create(dxr, kbs, path);
  }

  if (u_strcasecmp(extension.c_str(), u".txt", U_FOLD_CASE_DEFAULT) == 0) {
    return PlainTextFilePageSource::Create(dxr, kbs, path);
  }

  if (ImageFilePageSource::CanOpenFile(dxr, path)) {
    return ImageFilePageSource::Create(
      dxr, std::vector<std::filesystem::path> {path});
  }

  dprintf("Couldn't find handler for {}", path);

  return {nullptr};
}

}// namespace OpenKneeboard
