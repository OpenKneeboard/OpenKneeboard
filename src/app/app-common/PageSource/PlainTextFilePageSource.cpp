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
#include <OpenKneeboard/PlainTextFilePageSource.h>
#include <OpenKneeboard/PlainTextPageSource.h>
#include <OpenKneeboard/scope_guard.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <fstream>
#include <nlohmann/json.hpp>

using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Search;

namespace OpenKneeboard {

PlainTextFilePageSource::PlainTextFilePageSource(
  const DXResources& dxr,
  KneeboardState* kbs)
  : PageSourceWithDelegates(dxr, kbs),
    mPageSource(std::make_shared<PlainTextPageSource>(dxr, _("[empty file]"))) {
  this->SetDelegates({std::static_pointer_cast<IPageSource>(mPageSource)});
}

PlainTextFilePageSource::~PlainTextFilePageSource() {
  this->RemoveAllEventListeners();
}

std::shared_ptr<PlainTextFilePageSource> PlainTextFilePageSource::Create(
  const DXResources& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path) {
  std::shared_ptr<PlainTextFilePageSource> ret {
    new PlainTextFilePageSource(dxr, kbs)};
  if (!path.empty()) {
    ret->SetPath(path);
  }
  return ret;
}

std::filesystem::path PlainTextFilePageSource::GetPath() const {
  return mPath;
}

void PlainTextFilePageSource::SetPath(const std::filesystem::path& path) {
  if (path == mPath) {
    return;
  }
  mPath = std::filesystem::canonical(path);
  this->Reload();
}

void PlainTextFilePageSource::Reload() {
  scope_guard emitEvents([this]() {
    this->evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
    this->evNeedsRepaintEvent.Emit();
  });

  this->mQueryResult = {nullptr};

  if (!std::filesystem::is_regular_file(mPath)) {
    mPageSource->ClearText();
    return;
  }

  mPageSource->SetText(this->GetFileContent());
  mLastWriteTime = std::filesystem::last_write_time(mPath);
  this->SubscribeToChanges();
}

winrt::fire_and_forget PlainTextFilePageSource::SubscribeToChanges() noexcept {
  auto weakThis = this->weak_from_this();
  auto strongThis = this->shared_from_this();

  auto folder = co_await StorageFolder::GetFolderFromPathAsync(
    mPath.parent_path().wstring());
  mQueryResult = folder.CreateFileQuery();
  mQueryResult.ContentsChanged([weakThis](const auto&, const auto&) {
    auto strongThis = weakThis.lock();
    if (!strongThis) {
      return;
    }
    strongThis->OnFileModified();
  });
  // Must fetch results once to make the query active
  co_await mQueryResult.GetFilesAsync();
}

void PlainTextFilePageSource::OnFileModified() noexcept {
  if (!std::filesystem::is_regular_file(mPath)) {
    mPageSource->SetText({});
    mPageSource->SetPlaceholderText(_("[file deleted]"));
    this->evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
    return;
  }
  const auto newWriteTime = std::filesystem::last_write_time(mPath);
  if (newWriteTime == mLastWriteTime) {
    return;
  }

  mLastWriteTime = newWriteTime;
  mPageSource->SetText(this->GetFileContent());
  mPageSource->SetPlaceholderText(_("[empty file]"));
  this->evContentChangedEvent.Emit(ContentChangeType::Modified);
}

std::string PlainTextFilePageSource::GetFileContent() const {
  auto bytes = std::filesystem::file_size(mPath);
  std::string buffer;
  buffer.resize(bytes);
  size_t offset = 0;

  std::ifstream f(mPath, std::ios::in | std::ios::binary);
  if (!f.is_open()) {
    mPageSource->ClearText();
    return {};
  }
  while (bytes > 0) {
    f.read(&buffer.data()[offset], bytes);
    bytes -= f.gcount();
    offset += f.gcount();
  }

  size_t pos = 0;
  while ((pos = buffer.find("\r\n", pos)) != std::string::npos) {
    buffer.replace(pos, 2, "\n");
    pos++;
  }

  return buffer;
}

uint16_t PlainTextFilePageSource::GetPageCount() const {
  // Show placeholder "[empty file]" instead of 'no pages' error
  return std::max<uint16_t>(1, mPageSource->GetPageCount());
}

}// namespace OpenKneeboard
