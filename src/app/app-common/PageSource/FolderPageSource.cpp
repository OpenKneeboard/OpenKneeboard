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
#include <OpenKneeboard/FilePageSource.hpp>
#include <OpenKneeboard/FolderPageSource.hpp>

#include <shims/nlohmann/json.hpp>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <OpenKneeboard/dprint.hpp>

namespace OpenKneeboard {

FolderPageSource::FolderPageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : PageSourceWithDelegates(dxr, kbs), mDXR(dxr), mKneeboard(kbs) {
}

task<std::shared_ptr<FolderPageSource>> FolderPageSource::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path) {
  std::shared_ptr<FolderPageSource> ret {new FolderPageSource(dxr, kbs)};
  if (!path.empty()) {
    co_await ret->SetPath(path);
  }
  co_return ret;
}

FolderPageSource::~FolderPageSource() {
  this->RemoveAllEventListeners();
}

winrt::Windows::Foundation::IAsyncAction FolderPageSource::Reload() noexcept {
  const auto weakThis = this->weak_from_this();
  co_await mUIThread;
  const auto stayingAlive = this->shared_from_this();
  if (!stayingAlive) {
    co_return;
  }

  if (mPath.empty() || !std::filesystem::is_directory(mPath)) {
    EventDelay eventDelay;
    co_await this->SetDelegates({});
    evContentChangedEvent.Emit();
    co_return;
  }

  this->SubscribeToChanges();
  this->OnFileModified(mPath);
}

void FolderPageSource::SubscribeToChanges() {
  mWatcher = FilesystemWatcher::Create(mPath);
  AddEventListener(
    mWatcher->evFilesystemModifiedEvent, [weak = weak_from_this()](auto path) {
      if (auto self = weak.lock()) {
        self->OnFileModified(path);
      }
    });
}

winrt::fire_and_forget FolderPageSource::OnFileModified(
  std::filesystem::path directory) {
  if (directory != mPath) {
    co_return;
  }
  if (!std::filesystem::is_directory(directory)) {
    co_return;
  }
  decltype(mContents) newContents;
  bool modifiedOrNew = false;
  for (const auto& entry:
       std::filesystem::recursive_directory_iterator(directory)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    const auto path = entry.path();
    const auto mtime = entry.last_write_time();

    auto it = mContents.find(path);
    if (it != mContents.end()) {
      // All file-sourced tabs watch their own content
      newContents[path] = it->second;
      newContents[path].mModified = mtime;
      continue;
    }
    auto delegate = co_await FilePageSource::Create(mDXR, mKneeboard, path);
    if (delegate) {
      modifiedOrNew = true;
      newContents[path] = {mtime, delegate};
    }
  }

  if (newContents.size() == mContents.size() && !modifiedOrNew) {
    dprintf(L"No actual change to {}", mPath.wstring());
    co_return;
  }
  dprintf(L"Real change to {}", mPath.wstring());

  std::vector<std::shared_ptr<IPageSource>> delegates;
  for (const auto& [path, info]: newContents) {
    delegates.push_back(info.mDelegate);
  }

  EventDelay eventDelay;
  mContents = newContents;
  co_await this->SetDelegates(delegates);
}

std::filesystem::path FolderPageSource::GetPath() const {
  return mPath;
}

winrt::Windows::Foundation::IAsyncAction FolderPageSource::SetPath(
  std::filesystem::path path) {
  if (path == mPath) {
    co_return;
  }
  mPath = path;
  mWatcher = {};
  co_await this->Reload();
}

}// namespace OpenKneeboard
