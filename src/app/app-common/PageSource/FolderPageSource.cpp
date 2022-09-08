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
#include <OpenKneeboard/FilePageSource.h>
#include <OpenKneeboard/FolderPageSource.h>
#include <OpenKneeboard/dprint.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <nlohmann/json.hpp>

using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Search;

namespace OpenKneeboard {

FolderPageSource::FolderPageSource(const DXResources& dxr, KneeboardState* kbs)
  : PageSourceWithDelegates(dxr, kbs), mDXR(dxr), mKneeboard(kbs) {
}

std::shared_ptr<FolderPageSource> FolderPageSource::Create(
  const DXResources& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path) {
  std::shared_ptr<FolderPageSource> ret {new FolderPageSource(dxr, kbs)};
  if (!path.empty()) {
    ret->SetPath(path);
  }
  return ret;
}

FolderPageSource::~FolderPageSource() = default;

winrt::fire_and_forget FolderPageSource::Reload() noexcept {
  const auto weakThis = this->weak_from_this();
  co_await mUIThread;
  const auto strongThis = this->shared_from_this();
  if (!strongThis) {
    co_return;
  }

  if (mPath.empty() || !std::filesystem::is_directory(mPath)) {
    EventDelay eventDelay;
    this->SetDelegates({});
    evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
    co_return;
  }
  if ((!mQueryResult) || mPath != mQueryResult.Folder().Path()) {
    mQueryResult = nullptr;
    auto folder
      = co_await StorageFolder::GetFolderFromPathAsync(mPath.wstring());
    mQueryResult = folder.CreateFileQuery(CommonFileQuery::OrderByName);
    mQueryResult.ContentsChanged([=](const auto&, const auto&) {
      dprintf(L"Folder {} was modified", mPath.wstring());
      this->Reload();
    });
  }
  const auto files = co_await mQueryResult.GetFilesAsync();

  co_await mUIThread;

  std::vector<std::shared_ptr<IPageSource>> delegates;
  for (const auto& file: files) {
    std::filesystem::path path(std::wstring_view {file.Path()});
    auto delegate = FilePageSource::Get(mDXR, mKneeboard, path);
    if (delegate) {
      delegates.push_back(std::move(delegate));
    }
  }

  EventDelay eventDelay;
  this->SetDelegates(delegates);
}

std::filesystem::path FolderPageSource::GetPath() const {
  return mPath;
}

void FolderPageSource::SetPath(const std::filesystem::path& path) {
  if (path == mPath) {
    return;
  }
  mPath = path;
  mQueryResult = nullptr;
  this->Reload();
}

}// namespace OpenKneeboard
