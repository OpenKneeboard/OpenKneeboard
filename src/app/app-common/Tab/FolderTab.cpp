// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/FolderPageSource.hpp>
#include <OpenKneeboard/FolderTab.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <shims/nlohmann/json.hpp>

namespace OpenKneeboard {

FolderTab::FolderTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title)
  : TabBase(persistentID, title),
    PageSourceWithDelegates(dxr, kbs),
    mDXResources(dxr),
    mKneeboard(kbs) {}

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
  std::shared_ptr<FolderTab> ret {new FolderTab(dxr, kbs, persistentID, title)};
  co_await ret->SetPath(settings.at("Path").get<std::filesystem::path>());
  co_return ret;
}

FolderTab::~FolderTab() {}

nlohmann::json FolderTab::GetSettings() const { return {{"Path", GetPath()}}; }

std::string FolderTab::GetGlyph() const { return GetStaticGlyph(); }

std::string FolderTab::GetStaticGlyph() { return "\uE838"; }

task<void> FolderTab::Reload() { return mPageSource->Reload(); }

std::filesystem::path FolderTab::GetPath() const { return mPath; }

task<void> FolderTab::SetPath(std::filesystem::path path) {
  if (path == mPath) {
    co_return;
  }

  if (mPageSource) {
    co_await mPageSource->SetPath(path);
  } else {
    mPageSource =
      co_await FolderPageSource::Create(mDXResources, mKneeboard, path);
    co_await this->SetDelegates({mPageSource});
  }
  mPath = path;
}

}// namespace OpenKneeboard
