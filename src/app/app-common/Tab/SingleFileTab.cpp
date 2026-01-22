// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/ChromiumPageSource.hpp>
#include <OpenKneeboard/FilePageSource.hpp>
#include <OpenKneeboard/ImageFilePageSource.hpp>
#include <OpenKneeboard/PDFFilePageSource.hpp>
#include <OpenKneeboard/PlainTextFilePageSource.hpp>
#include <OpenKneeboard/SingleFileTab.hpp>

#include <OpenKneeboard/scope_exit.hpp>

#include <shims/nlohmann/json.hpp>

namespace OpenKneeboard {

SingleFileTab::SingleFileTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title)
  : TabBase(persistentID, title),
    PageSourceWithDelegates(dxr, kbs),
    mDXR(dxr),
    mKneeboard(kbs) {}

task<std::shared_ptr<SingleFileTab>> SingleFileTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path) {
  return Create(dxr, kbs, winrt::guid {}, to_utf8(path.stem()), path);
}

task<std::shared_ptr<SingleFileTab>> SingleFileTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const nlohmann::json& settings) {
  return Create(
    dxr,
    kbs,
    persistentID,
    title,
    settings.at("Path").get<std::filesystem::path>());
}

task<std::shared_ptr<SingleFileTab>> SingleFileTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const std::filesystem::path& path) {
  std::shared_ptr<SingleFileTab> ret {
    new SingleFileTab(dxr, kbs, persistentID, title)};
  co_await ret->SetPath(path);
  co_return ret;
}

SingleFileTab::~SingleFileTab() {}

nlohmann::json SingleFileTab::GetSettings() const {
  return {{"Path", GetPath()}};
}

std::string SingleFileTab::GetGlyph() const {
  switch (mKind) {
    case Kind::PDFFile:
      return "\uEA90";
    case Kind::ImageFile:
      return "\uE91B";
    case Kind::PlainTextFile:
      return "\uE8A5";
    case Kind::HTMLFile:
      return "\ueb41";
    default:
      return "";
  }
}

std::string SingleFileTab::GetStaticGlyph() { return "\ue8e5"; }

std::filesystem::path SingleFileTab::GetPath() const { return mPath; }

task<void> SingleFileTab::Reload() {
  mKind = Kind::Unknown;
  auto path = std::exchange(mPath, {});
  co_await this->SetPath(path);
}

task<void> SingleFileTab::SetPath(std::filesystem::path rawPath) {
  auto path = rawPath;
  if (std::filesystem::exists(path)) {
    path = std::filesystem::canonical(path);
  }
  if (path == mPath) {
    co_return;
  }
  mPath = path;

  auto delegate = co_await FilePageSource::Create(mDXR, mKneeboard, mPath);
  if (!delegate) {
    co_return;
  }

  if (std::dynamic_pointer_cast<PDFFilePageSource>(delegate)) {
    mKind = Kind::PDFFile;
  } else if (std::dynamic_pointer_cast<PlainTextFilePageSource>(delegate)) {
    mKind = Kind::PlainTextFile;
  } else if (std::dynamic_pointer_cast<ImageFilePageSource>(delegate)) {
    mKind = Kind::ImageFile;
  } else if (std::dynamic_pointer_cast<ChromiumPageSource>(delegate)) {
    mKind = Kind::HTMLFile;
  }

  co_await this->SetDelegates({delegate});
}

}// namespace OpenKneeboard
