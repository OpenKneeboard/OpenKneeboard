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
#include <OpenKneeboard/Config.h>
#include <OpenKneeboard/DoodleRenderer.h>
#include <OpenKneeboard/EndlessNotebookTab.h>
#include <OpenKneeboard/FilePageSource.h>

#include <nlohmann/json.hpp>

namespace OpenKneeboard {

EndlessNotebookTab::EndlessNotebookTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const std::filesystem::path& path)
  : TabBase(persistentID, title), mDXR(dxr), mKneeboard(kbs) {
  mDoodles = std::make_unique<DoodleRenderer>(dxr, kbs);
  this->SetPath(path);

  AddEventListener(
    mDoodles->evAddedPageEvent, this->evAvailableFeaturesChangedEvent);
  AddEventListener(mDoodles->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
}

EndlessNotebookTab::EndlessNotebookTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path)
  : EndlessNotebookTab(dxr, kbs, winrt::guid {}, to_utf8(path.stem()), path) {
}

EndlessNotebookTab::EndlessNotebookTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const nlohmann::json& settings)
  : EndlessNotebookTab(
    dxr,
    kbs,
    persistentID,
    title,
    settings.at("Path").get<std::filesystem::path>()) {
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

void EndlessNotebookTab::SetPath(const std::filesystem::path& rawPath) {
  auto path = rawPath;
  if (std::filesystem::exists(path)) {
    path = std::filesystem::canonical(path);
  }
  if (path == mPath) {
    return;
  }
  mPath = path;

  this->Reload();
}

void EndlessNotebookTab::Reload() {
  auto delegate = FilePageSource::Create(mDXR, mKneeboard, mPath);
  if (!delegate) {
    mSource = {};
    mPageCount = 0;
    evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
    return;
  }

  mPageCount = 1;
  mSource = delegate;
  evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
}

PageIndex EndlessNotebookTab::GetPageCount() const {
  return mPageCount;
}

D2D1_SIZE_U EndlessNotebookTab::GetNativeContentSize(PageIndex pageIndex) {
  if (pageIndex < mPageCount) {
    return mSource->GetNativeContentSize(0);
  }

  return {ErrorRenderWidth, ErrorRenderHeight};
}

void EndlessNotebookTab::RenderPage(
  RenderTargetID rtid,
  ID2D1DeviceContext* d2d,
  PageIndex pageIndex,
  const D2D1_RECT_F& rect) {
  if (!mSource) {
    return;
  }

  mSource->RenderPage(rtid, d2d, 0, rect);
  mDoodles->Render(d2d, pageIndex, rect);
}

void EndlessNotebookTab::PostCursorEvent(
  EventContext ec,
  const CursorEvent& ce,
  PageIndex pageIndex) {
  if (!mSource) {
    return;
  }

  mDoodles->PostCursorEvent(
    ec, ce, pageIndex, mSource->GetNativeContentSize(0));
  if (mDoodles->HaveDoodles(pageIndex) && pageIndex == mPageCount - 1) {
    mPageCount++;
    evPageAppendedEvent.Emit(SuggestedPageAppendAction::KeepOnCurrentPage);
  }
}

bool EndlessNotebookTab::CanClearUserInput(PageIndex index) const {
  return mDoodles->HaveDoodles(index);
}

bool EndlessNotebookTab::CanClearUserInput() const {
  return mDoodles->HaveDoodles();
}

void EndlessNotebookTab::ClearUserInput(PageIndex index) {
  mDoodles->ClearPage(index);
  evAvailableFeaturesChangedEvent.Emit();
}

void EndlessNotebookTab::ClearUserInput() {
  mDoodles->Clear();
  evAvailableFeaturesChangedEvent.Emit();
}

}// namespace OpenKneeboard
