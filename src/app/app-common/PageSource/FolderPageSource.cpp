// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/FileHash.hpp>
#include <OpenKneeboard/FilePageSource.hpp>
#include <OpenKneeboard/FolderPageSource.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <shims/nlohmann/json.hpp>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

namespace OpenKneeboard {

FolderPageSource::FolderPageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : PageSourceWithDelegates(dxr, kbs),
    mDXR(dxr),
    mKneeboard(kbs) {}

task<std::shared_ptr<FolderPageSource>> FolderPageSource::Create(
  audited_ptr<DXResources> dxr,
  KneeboardState* kbs,
  std::filesystem::path path) {
  std::shared_ptr<FolderPageSource> ret {new FolderPageSource(dxr, kbs)};
  if (!path.empty()) {
    co_await ret->SetPath(path);
  }
  co_return ret;
}

FolderPageSource::~FolderPageSource() { this->RemoveAllEventListeners(); }

task<void> FolderPageSource::Reload() noexcept {
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

OpenKneeboard::fire_and_forget FolderPageSource::OnFileModified(
  std::filesystem::path directory) {
  const auto keepAlive = shared_from_this();
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
    dprint(L"No actual change to {}", mPath.wstring());
    co_return;
  }
  dprint(L"Real change to {}", mPath.wstring());

  std::vector<std::shared_ptr<IPageSource>> delegates;
  for (const auto& [path, info]: newContents) {
    delegates.push_back(info.mDelegate);
  }

  EventDelay eventDelay;
  mContents = newContents;
  co_await this->SetDelegates(delegates);
}

std::filesystem::path FolderPageSource::GetPath() const { return mPath; }

task<void> FolderPageSource::SetPath(std::filesystem::path path) {
  if (path == mPath) {
    co_return;
  }
  mPath = path;
  mWatcher = {};
  co_await this->Reload();
}

std::optional<std::pair<std::filesystem::path, PageIndex>>
FolderPageSource::GetFileAndLocalIndexForPageID(PageID id) const {
  for (const auto& [absPath, info]: mContents) {
    const auto pageIDs = info.mDelegate->GetPageIDs();
    for (PageIndex i = 0; i < static_cast<PageIndex>(pageIDs.size()); ++i) {
      if (pageIDs[i] == id) {
        return std::make_pair(absPath.lexically_relative(mPath), i);
      }
    }
  }
  return std::nullopt;
}

std::optional<PageID> FolderPageSource::GetPageIDForRelativeFile(
  const std::filesystem::path& relativeFile,
  PageIndex localIndex) const {
  const auto full = (mPath / relativeFile).lexically_normal();
  const auto it = mContents.find(full);
  if (it == mContents.end()) {
    return std::nullopt;
  }
  const auto pageIDs = it->second.mDelegate->GetPageIDs();
  if (localIndex >= static_cast<PageIndex>(pageIDs.size())) {
    return std::nullopt;
  }
  return pageIDs[localIndex];
}

std::optional<nlohmann::json> FolderPageSource::GetPersistentIDForPage(
  PageID id) const {
  const auto info = this->GetFileAndLocalIndexForPageID(id);
  if (!info) {
    return std::nullopt;
  }
  const auto absFile = mPath / info->first;
  const auto hash = ComputeFileHash(absFile);
  if (!hash) {
    return std::nullopt;
  }
  return nlohmann::json {
    {"File", info->first.generic_string()},
    {"FileHash", hash},
    {"LocalPageIndex", info->second},
  };
}

std::optional<PageID> FolderPageSource::GetPageIDFromPersistentID(
  const nlohmann::json& id) const {
  if (!id.contains("File") || !id.contains("FileHash")
      || !id.contains("LocalPageIndex")) {
    return std::nullopt;
  }
  const std::filesystem::path relFile {id.at("File").get<std::string>()};
  const auto storedHash = id.at("FileHash").get<uint64_t>();
  const auto absFile = mPath / relFile;
  std::error_code ec;
  if (!std::filesystem::exists(absFile, ec) || ec) {
    return std::nullopt;
  }
  const auto currentHash = ComputeFileHash(absFile);
  if (!currentHash || currentHash != storedHash) {
    return std::nullopt;
  }
  return this->GetPageIDForRelativeFile(
    relFile, id.at("LocalPageIndex").get<PageIndex>());
}

}// namespace OpenKneeboard
