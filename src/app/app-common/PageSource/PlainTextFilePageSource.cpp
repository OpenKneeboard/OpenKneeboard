// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/PlainTextFilePageSource.hpp>
#include <OpenKneeboard/PlainTextPageSource.hpp>

#include <OpenKneeboard/scope_exit.hpp>

#include <shims/nlohmann/json.hpp>

#include <winrt/Windows.Foundation.h>

#include <fstream>

namespace OpenKneeboard {

PlainTextFilePageSource::PlainTextFilePageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : PageSourceWithDelegates(dxr, kbs),
    mPageSource(
      std::make_shared<PlainTextPageSource>(dxr, kbs, _("[empty file]"))) {}

PlainTextFilePageSource::~PlainTextFilePageSource() {
  this->RemoveAllEventListeners();
}

task<std::shared_ptr<PlainTextFilePageSource>> PlainTextFilePageSource::Create(
  audited_ptr<DXResources> dxr,
  KneeboardState* kbs,
  std::filesystem::path path) {
  std::shared_ptr<PlainTextFilePageSource> ret {
    new PlainTextFilePageSource(dxr, kbs)};
  co_await ret->SetDelegates(
    {std::static_pointer_cast<IPageSource>(ret->mPageSource)});
  if (!path.empty()) {
    ret->SetPath(path);
  }
  co_return ret;
}

std::filesystem::path PlainTextFilePageSource::GetPath() const { return mPath; }

void PlainTextFilePageSource::SetPath(const std::filesystem::path& path) {
  OPENKNEEBOARD_TraceLoggingScope(
    "PlainTextFilePageSource::SetPath()",
    TraceLoggingValue(path.c_str(), "Path"));
  if (path == mPath) {
    return;
  }
  mPath = std::filesystem::canonical(path);
  this->Reload();
}

void PlainTextFilePageSource::Reload() {
  scope_exit emitEvents([this]() {
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
    dprint(L"Failed to open {}", mPath.wstring());
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
