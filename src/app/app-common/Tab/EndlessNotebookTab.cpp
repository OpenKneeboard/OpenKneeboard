// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/DoodleRenderer.hpp>
#include <OpenKneeboard/EndlessNotebookTab.hpp>
#include <OpenKneeboard/FilePageSource.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/scope_exit.hpp>

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

task<std::shared_ptr<EndlessNotebookTab>> EndlessNotebookTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path) {
  std::shared_ptr<EndlessNotebookTab> ret {
    new EndlessNotebookTab(dxr, kbs, winrt::guid {}, to_utf8(path.stem()))};
  co_await ret->SetPath(path);
  co_return ret;
}

task<void> EndlessNotebookTab::DisposeAsync() noexcept {
  const auto disposing = co_await mDisposal.StartOnce();
  if (!disposing) {
    co_return;
  }

  auto child = std::dynamic_pointer_cast<IHasDisposeAsync>(mSource);
  if (!child) {
    co_return;
  }

  co_await child->DisposeAsync();
}

task<std::shared_ptr<EndlessNotebookTab>> EndlessNotebookTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title,
  const nlohmann::json& settings) {
  std::shared_ptr<EndlessNotebookTab> ret(
    new EndlessNotebookTab(dxr, kbs, persistentID, title));
  co_await ret->SetPath(settings.at("Path"));
  co_return ret;
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

task<void> EndlessNotebookTab::Reload() {
  const scope_exit contentChanged([this]() { evContentChangedEvent.Emit(); });

  auto path = std::exchange(mPath, {});
  co_await this->SetPath(path);
}

task<void> EndlessNotebookTab::SetPath(std::filesystem::path rawPath) {
  auto path = rawPath;
  if (std::filesystem::exists(path)) {
    path = std::filesystem::canonical(path);
  }
  if (path == mPath) {
    co_return;
  }
  mPath = path;
  if (path.empty()) {
    co_return;
  }

  auto delegate = co_await FilePageSource::Create(mDXR, mKneeboard, mPath);
  if (!delegate) {
    co_return;
  }

  mSource = nullptr;
  mSource = delegate;
  mDoodles->Clear();

  AddEventListener(mSource->evContentChangedEvent, [weak = weak_from_this()]() {
    if (auto self = weak.lock()) {
      self->OnSourceContentChanged();
    }
  });

  if (delegate->GetPageCount() == 0) {
    co_return;
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

std::optional<PreferredSize> EndlessNotebookTab::GetPreferredSize(PageID) {
  if (mSource) {
    return mSource->GetPreferredSize(mSourcePageID);
  }

  return std::nullopt;
}

task<void> EndlessNotebookTab::RenderPage(
  RenderContext rc,
  PageID pageID,
  PixelRect rect) {
  if (!mSource) {
    co_return;
  }

  co_await mSource->RenderPage(rc, mSourcePageID, rect);
  mDoodles->Render(rc.GetRenderTarget(), pageID, rect);
}

void EndlessNotebookTab::PostCursorEvent(
  KneeboardViewID view,
  const CursorEvent& ce,
  PageID pageID) {
  if (!mSource) {
    return;
  }

  const auto contentSize = mSource->GetPreferredSize(mSourcePageID);
  if (!contentSize.has_value()) {
    return;
  }

  mDoodles->PostCursorEvent(view, ce, pageID, contentSize->mPixelSize);
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
