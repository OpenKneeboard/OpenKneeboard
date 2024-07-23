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
#include <OpenKneeboard/FolderPageSource.hpp>
#include <OpenKneeboard/FolderTab.hpp>

#include <shims/nlohmann/json.hpp>

#include <OpenKneeboard/dprint.hpp>

namespace OpenKneeboard {

FolderTab::FolderTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const std::filesystem::path& path)
  : TabBase(persistentID, title),
    PageSourceWithDelegates(dxr, kbs),
    mPageSource(FolderPageSource::Create(dxr, kbs, path)),
    mPath {path} {
  this->SetDelegatesFromEmpty({mPageSource});
}

FolderTab::FolderTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path)
  : FolderTab(dxr, kbs, winrt::guid {}, to_utf8(path.filename()), path) {
}

FolderTab::FolderTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const nlohmann::json& settings)
  : FolderTab(
      dxr,
      kbs,
      persistentID,
      title,
      settings.at("Path").get<std::filesystem::path>()) {
}

FolderTab::~FolderTab() {
}

nlohmann::json FolderTab::GetSettings() const {
  return {{"Path", GetPath()}};
}

std::string FolderTab::GetGlyph() const {
  return GetStaticGlyph();
}

std::string FolderTab::GetStaticGlyph() {
  return "\uE838";
}

winrt::Windows::Foundation::IAsyncAction FolderTab::Reload() {
  return mPageSource->Reload();
}

std::filesystem::path FolderTab::GetPath() const {
  return mPath;
}

winrt::Windows::Foundation::IAsyncAction FolderTab::SetPath(
  std::filesystem::path path) {
  if (path == mPath) {
    co_return;
  }
  co_await mPageSource->SetPath(path);
  mPath = path;
}

}// namespace OpenKneeboard
