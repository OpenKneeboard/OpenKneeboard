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
#include <OpenKneeboard/config.h>

#include <OpenKneeboard/DoodleRenderer.h>
#include <OpenKneeboard/EndlessNotebookTab.h>
#include <OpenKneeboard/FilePageSource.h>

#include <OpenKneeboard/scope_exit.h>

#include <shims/nlohmann/json.hpp>

namespace OpenKneeboard {

EndlessNotebookTab::EndlessNotebookTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title)
  : TabBase(persistentID, title), mDXR(dxr), mKneeboard(kbs) {
  mDoodles = std::make_unique<DoodleRenderer>(dxr, kbs);

  AddEventListener(
    mDoodles->evAddedPageEvent, this->evAvailableFeaturesChangedEvent);
  AddEventListener(mDoodles->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
}

std::shared_ptr<EndlessNotebookTab> EndlessNotebookTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path) {
  auto ret = std::shared_ptr<EndlessNotebookTab>(
    new EndlessNotebookTab(dxr, kbs, winrt::guid {}, to_utf8(path.stem())));
  ret->SetPathFromEmpty(path);
  return ret;
}

std::shared_ptr<EndlessNotebookTab> EndlessNotebookTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const nlohmann::json& settings) {
  auto ret = std::shared_ptr<EndlessNotebookTab>(
    new EndlessNotebookTab(dxr, kbs, persistentID, title));
  ret->SetPathFromEmpty(settings.at("Path"));
  return ret;
}

EndlessNotebookTab::~EndlessNotebookTab() {
  this->RemoveAllEventListeners();
}

nlohmann::json EndlessNotebookTab::GetSettings() const {
  return {{"Path", GetPath()}};
}

std::string EndlessNotebookTab::GetGlyph() const {
  return GetStaticGlyph();
}

std::string EndlessNotebookTab::GetStaticGlyph() {
  return "\ue8ed";// RepeatOne
}

std::filesystem::path EndlessNotebookTab::GetPath() const {
  return mPath;
}

winrt::Windows::Foundation::IAsyncAction EndlessNotebookTab::SetPath(
  std::filesystem::path rawPath) {
  auto path = rawPath;
  if (std::filesystem::exists(path)) {
    path = std::filesystem::canonical(path);
  }
  if (path == mPath) {
    co_return;
  }
  mPath = path;

  co_await this->Reload();
}

winrt::Windows::Foundation::IAsyncAction EndlessNotebookTab::Reload() {
  const scope_exit contentChanged([this]() { evContentChangedEvent.Emit(); });

  mPageIDs.clear();
  mSourcePageID = {nullptr};
  mSource = {};
  mDoodles->Clear();

  auto path = mPath;
  mPath.clear();
  this->SetPathFromEmpty(path);
  co_return;
}

void EndlessNotebookTab::SetPathFromEmpty(const std::filesystem::path& path) {
  assert(mPath.empty());

  auto delegate = FilePageSource::Create(mDXR, mKneeboard, mPath);
  if (!delegate) {
    return;
  }

  mSource = delegate;

  AddEventListener(mSource->evContentChangedEvent, [weak = weak_from_this()]() {
    if (auto self = weak.lock()) {
      self->OnSourceContentChanged();
    }
  });

  if (delegate->GetPageCount() == 0) {
    return;
  }
  mPageIDs = {PageID {}};
  mSourcePageID = mSource->GetPageIDs().front();
}

void EndlessNotebookTab::OnSourceContentChanged() {
  const auto ids = mSource->GetPageIDs();
  if (ids.empty()) {
    mDoodles->Clear();
    mPageIDs.clear();
    mSourcePageID = {nullptr};
    evContentChangedEvent.Emit();
    return;
  }

  if (mSourcePageID == ids.front()) {
    return;
  }

  mDoodles->Clear();
  mSourcePageID = ids.front();
  mPageIDs = {PageID {}};
  evContentChangedEvent.Emit();
}

PageIndex EndlessNotebookTab::GetPageCount() const {
  return mPageIDs.size();
}

std::vector<PageID> EndlessNotebookTab::GetPageIDs() const {
  return mPageIDs;
}

PreferredSize EndlessNotebookTab::GetPreferredSize(PageID) {
  if (mSource) {
    return mSource->GetPreferredSize(mSourcePageID);
  }

  return {
    ErrorRenderSize,
    ScalingKind::Vector,
  };
}

void EndlessNotebookTab::RenderPage(
  const RenderContext& rc,
  PageID pageID,
  const PixelRect& rect) {
  if (!mSource) {
    return;
  }

  mSource->RenderPage(rc, mSourcePageID, rect);
  mDoodles->Render(rc.GetRenderTarget(), pageID, rect);
}

void EndlessNotebookTab::PostCursorEvent(
  EventContext ec,
  const CursorEvent& ce,
  PageID pageID) {
  if (!mSource) {
    return;
  }

  mDoodles->PostCursorEvent(
    ec, ce, pageID, mSource->GetPreferredSize(mSourcePageID).mPixelSize);
  if (mDoodles->HaveDoodles(pageID) && pageID == mPageIDs.back()) {
    mPageIDs.push_back({});
    evPageAppendedEvent.Emit(SuggestedPageAppendAction::KeepOnCurrentPage);
  }
}

bool EndlessNotebookTab::CanClearUserInput(PageID id) const {
  return mDoodles->HaveDoodles(id);
}

bool EndlessNotebookTab::CanClearUserInput() const {
  return mDoodles->HaveDoodles();
}

void EndlessNotebookTab::ClearUserInput(PageID id) {
  mDoodles->ClearPage(id);
  evAvailableFeaturesChangedEvent.Emit();
}

void EndlessNotebookTab::ClearUserInput() {
  mDoodles->Clear();
  evAvailableFeaturesChangedEvent.Emit();
}

}// namespace OpenKneeboard
