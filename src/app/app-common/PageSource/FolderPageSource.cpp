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

FolderPageSource::~FolderPageSource() {
  this->RemoveAllEventListeners();
}

winrt::fire_and_forget FolderPageSource::Reload() noexcept {
  const auto weakThis = this->weak_from_this();
  co_await mUIThread;
  const auto stayingAlive = this->shared_from_this();
  if (!stayingAlive) {
    co_return;
  }

  if (mPath.empty() || !std::filesystem::is_directory(mPath)) {
    EventDelay eventDelay;
    this->SetDelegates({});
    evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
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

void FolderPageSource::OnFileModified(const std::filesystem::path& directory) {
  if (directory != mPath) {
    return;
  }
  if (!std::filesystem::is_directory(directory)) {
    return;
  }
  decltype(mContents) newContents;
  bool modifiedOrNew = false;
  for (const auto entry:
       std::filesystem::recursive_directory_iterator(directory)) {
    const auto path = entry.path();
    const auto mtime = entry.last_write_time();

    auto it = mContents.find(path);
    if (it != mContents.end() && it->second.mModified == mtime) {
      newContents[path] = it->second;
      continue;
    }
    auto delegate = FilePageSource::Create(mDXR, mKneeboard, path);
    if (delegate) {
      modifiedOrNew = true;
      newContents[path] = {mtime, delegate};
    }
  }

  if (newContents.size() == mContents.size() && !modifiedOrNew) {
    dprintf(L"No actual change to {}", mPath.wstring());
    return;
  }
  dprintf(L"Real change to {}", mPath.wstring());

  std::vector<std::shared_ptr<IPageSource>> delegates;
  for (const auto& [path, info]: newContents) {
    delegates.push_back(info.mDelegate);
  }

  EventDelay eventDelay;
  mContents = newContents;
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
  mWatcher = {};
  this->Reload();
}

}// namespace OpenKneeboard
