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
  std::string_view title)
  : TabBase(persistentID, title),
    PageSourceWithDelegates(dxr, kbs),
    mDXResources(dxr),
    mKneeboard(kbs) {
}

task<std::shared_ptr<FolderTab>> FolderTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path) {
  std::shared_ptr<FolderTab> ret {
    new FolderTab(dxr, kbs, winrt::guid {}, to_utf8(path.filename()))};
  co_await ret->SetPath(path);
  co_return ret;
}

task<std::shared_ptr<FolderTab>> FolderTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const nlohmann::json& settings) {
  std::shared_ptr<FolderTab> ret {
    new FolderTab(dxr, kbs, persistentID, settings)};
  co_await ret->SetPath(settings.at("Path").get<std::filesystem::path>());
  co_return ret;
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

  if (mPageSource) {
    co_await mPageSource->SetPath(path);
  } else {
    mPageSource
      = co_await FolderPageSource::Create(mDXResources, mKneeboard, path);
    co_await this->SetDelegates({mPageSource});
  }
  mPath = path;
}

}// namespace OpenKneeboard
