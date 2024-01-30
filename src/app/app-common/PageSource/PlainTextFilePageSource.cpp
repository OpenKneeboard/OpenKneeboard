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
#include <OpenKneeboard/weak_wrap.h>

#include <winrt/Windows.Foundation.h>

#include <nlohmann/json.hpp>

#include <fstream>

namespace OpenKneeboard {

PlainTextFilePageSource::PlainTextFilePageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : PageSourceWithDelegates(dxr, kbs),
    mPageSource(
      std::make_shared<PlainTextPageSource>(dxr, kbs, _("[empty file]"))) {
  this->SetDelegates({std::static_pointer_cast<IPageSource>(mPageSource)});
}

PlainTextFilePageSource::~PlainTextFilePageSource() {
  this->RemoveAllEventListeners();
}

std::shared_ptr<PlainTextFilePageSource> PlainTextFilePageSource::Create(
  const audited_ptr<DXResources>& dxr,
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
    this->evContentChangedEvent.Emit();
    this->evNeedsRepaintEvent.Emit();
  });

  this->mWatcher = {nullptr};

  if (!std::filesystem::is_regular_file(mPath)) {
    mPageSource->ClearText();
    return;
  }

  mPageSource->SetText(this->GetFileContent());
  this->SubscribeToChanges();
}

void PlainTextFilePageSource::SubscribeToChanges() noexcept {
  mWatcher = FilesystemWatcher::Create(mPath);
  AddEventListener(
    mWatcher->evFilesystemModifiedEvent, [weak = weak_from_this()](auto path) {
      if (auto self = weak.lock()) {
        self->OnFileModified(path);
      }
    });
}

void PlainTextFilePageSource::OnFileModified(
  const std::filesystem::path& path) noexcept {
  if (path != mPath) {
    return;
  }

  if (!std::filesystem::is_regular_file(mPath)) {
    mPageSource->SetText({});
    mPageSource->SetPlaceholderText(_("[file deleted]"));
    this->evContentChangedEvent.Emit();
    return;
  }

  const auto newWriteTime = std::filesystem::last_write_time(mPath);

  mPageSource->SetText(this->GetFileContent());
  mPageSource->SetPlaceholderText(_("[empty file]"));
  this->evContentChangedEvent.Emit();
}

std::string PlainTextFilePageSource::GetFileContent() const {
  auto bytes = std::filesystem::file_size(mPath);
  if (bytes == 0) {
    return {};
  }
  std::string buffer;
  buffer.resize(bytes);
  size_t offset = 0;

  std::ifstream f(mPath, std::ios::in | std::ios::binary);
  if (!f.is_open()) {
    dprintf(L"Failed to open {}", mPath.wstring());
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

PageIndex PlainTextFilePageSource::GetPageCount() const {
  // Show placeholder "[empty file]" instead of 'no pages' error
  return std::max<PageIndex>(1, mPageSource->GetPageCount());
}

}// namespace OpenKneeboard
