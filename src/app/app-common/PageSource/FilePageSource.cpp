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
#include <shims/winrt/base.h>

#include <ranges>

#include <icu.h>

namespace OpenKneeboard {

static std::optional<std::wstring> variable_sized_string_mem_fn(
  auto self,
  auto impl) {
  UINT bufSize = 0;
  const auto fn = std::bind_front(impl, self);
  HRESULT result = fn(0, nullptr, &bufSize);
  if (result != S_OK) {
    return std::nullopt;
  }

  std::wstring buf;
  buf.resize(bufSize, L'\0');
  result = fn(bufSize, buf.data(), &bufSize);
  if (result != S_OK) {
    return std::nullopt;
  }
  buf.pop_back();// trailing null
  return buf;
}

std::vector<std::string> FilePageSource::GetSupportedExtensions(
  const audited_ptr<DXResources>& dxr) noexcept {
  std::vector<std::string> ret {".txt", ".pdf"};

  winrt::com_ptr<IEnumUnknown> enumerator;
  winrt::check_hresult(
    dxr->mWIC->CreateComponentEnumerator(WICDecoder, 0, enumerator.put()));

  winrt::com_ptr<IUnknown> it;
  ULONG fetched;
  while (enumerator->Next(1, it.put(), &fetched) == S_OK) {
    const scope_guard clearIt([&]() { it = {nullptr}; });
    const auto info = it.as<IWICBitmapCodecInfo>();

    CLSID clsID {};
    winrt::check_hresult(info->GetCLSID(&clsID));

    const auto name = variable_sized_string_mem_fn(
      info, &IWICBitmapCodecInfo::GetFriendlyName);
    const auto author
      = variable_sized_string_mem_fn(info, &IWICBitmapCodecInfo::GetAuthor);
    const auto extensions = variable_sized_string_mem_fn(
      info, &IWICBitmapCodecInfo::GetFileExtensions);

    if (!(name && author && extensions)) {
      dprintf(
        "WARNING: Failed to get necessary information for WIC component {}",
        winrt::guid {clsID});
      OPENKNEEBOARD_BREAK;
      continue;
    }

    dprintf(
      L"Found WIC codec '{}' ({}) by '{}'; extensions: {}",
      *name,
      winrt::guid {clsID},
      *author,
      *extensions);

    DWORD status {};
    winrt::check_hresult(info->GetSigningStatus(&status));
    if (
      ((status & WICComponentSigned) == 0)
      && ((status & WICComponentSafe) == 0)) {
      dprintf(
        "WARNING: Skipping codec - unsafe status {:#018x}",
        std::bit_cast<uint32_t>(status));
      continue;
    }
    for (const auto& range: std::views::split(*extensions, L',')) {
      ret.push_back(
        winrt::to_string(std::wstring_view(range.begin(), range.end())));
    }
  }

  return ret;
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
