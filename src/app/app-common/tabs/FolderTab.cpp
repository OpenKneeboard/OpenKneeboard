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
#include <OpenKneeboard/FolderTab.h>
#include <OpenKneeboard/ImagePagesSource.h>
#include <OpenKneeboard/NavigationTab.h>
#include <OpenKneeboard/dprint.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <nlohmann/json.hpp>

using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Search;

namespace OpenKneeboard {

FolderTab::FolderTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view /* title */,
  const std::filesystem::path& path)
  : TabWithDoodles(dxr, kbs),
    mDXR(dxr),
    mPageSource(std::make_unique<ImagePagesSource>(dxr)),
    mPath {path} {
  this->Reload();
}

FolderTab::FolderTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view title,
  const nlohmann::json& settings)
  : FolderTab(
    dxr,
    kbs,
    title,
    settings.at("Path").get<std::filesystem::path>()) {
}

FolderTab::~FolderTab() {
}

nlohmann::json FolderTab::GetSettings() const {
  return {{"Path", GetPath()}};
}

utf8_string FolderTab::GetGlyph() const {
  return "\uE838";
}

utf8_string FolderTab::GetTitle() const {
  return mPath.filename();
}

void FolderTab::Reload() {
  this->ReloadImpl();
}

winrt::fire_and_forget FolderTab::ReloadImpl() noexcept {
  co_await mUIThread;

  if (mPath.empty() || !std::filesystem::is_directory(mPath)) {
    mPageSource->SetPaths({});
    evFullyReplacedEvent.Emit();
    co_return;
  }
  if ((!mQueryResult) || mPath != mQueryResult.Folder().Path()) {
    mQueryResult = nullptr;
    auto folder
      = co_await StorageFolder::GetFolderFromPathAsync(mPath.wstring());
    mQueryResult = folder.CreateFileQuery(CommonFileQuery::OrderByName);
    mQueryResult.ContentsChanged(
      [this](const auto&, const auto&) { this->ReloadImpl(); });
  }
  const auto files = co_await mQueryResult.GetFilesAsync();

  co_await winrt::resume_background();

  std::vector<std::filesystem::path> paths;
  for (const auto& file: files) {
    std::filesystem::path path(std::wstring_view {file.Path()});
    if (!mPageSource->CanOpenFile(path)) {
      continue;
    }
    paths.push_back(path);
  }

  co_await mUIThread;

  mPageSource->SetPaths(paths);
  evFullyReplacedEvent.Emit();
}

uint16_t FolderTab::GetPageCount() const {
  return mPageSource->GetPageCount();
}

D2D1_SIZE_U FolderTab::GetNativeContentSize(uint16_t index) {
  return mPageSource->GetNativeContentSize(index);
}

void FolderTab::RenderPageContent(
  ID2D1DeviceContext* ctx,
  uint16_t index,
  const D2D1_RECT_F& rect) {
  mPageSource->RenderPage(ctx, index, rect);
}

std::filesystem::path FolderTab::GetPath() const {
  return mPath;
}

void FolderTab::SetPath(const std::filesystem::path& path) {
  if (path == mPath) {
    return;
  }
  mPath = path;
  mQueryResult = nullptr;
  this->Reload();
}

bool FolderTab::IsNavigationAvailable() const {
  return mPageSource->GetPageCount() > 2;
}

std::shared_ptr<ITab> FolderTab::CreateNavigationTab(uint16_t currentPage) {
  std::vector<NavigationTab::Entry> entries;

  const auto paths = mPageSource->GetPaths();

  for (uint16_t i = 0; i < paths.size(); ++i) {
    entries.push_back({paths.at(i).stem(), i});
  }

  return std::make_shared<NavigationTab>(
    mDXR, this, entries, this->GetNativeContentSize(currentPage));
}

}// namespace OpenKneeboard
