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
#include <OpenKneeboard/CachedLayer.h>
#include <OpenKneeboard/CursorClickableRegions.h>
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/DoodleRenderer.h>
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/FilesystemWatcher.h>
#include <OpenKneeboard/LaunchURI.h>
#include <OpenKneeboard/NavigationTab.h>
#include <OpenKneeboard/PDFFilePageSource.h>
#include <OpenKneeboard/PDFNavigation.h>
#include <OpenKneeboard/RuntimeFiles.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/utf8.h>

#include <shims/winrt/base.h>

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Data.Pdf.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <wil/cppwinrt.h>
#include <wil/cppwinrt_helpers.h>

#include <windows.data.pdf.interop.h>

#include <nlohmann/json.hpp>

#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#include <inttypes.h>

using namespace winrt::Windows::Data::Pdf;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;

namespace OpenKneeboard {

struct PDFFilePageSource::Impl final {
  using LinkHandler = CursorClickableRegions<PDFNavigation::Link>;

  audited_ptr<DXResources> mDXR;
  std::filesystem::path mPath;
  std::shared_ptr<Filesystem::TemporaryCopy> mCopy;

  PdfDocument mPDFDocument {nullptr};
  winrt::com_ptr<IPdfRendererNative> mPDFRenderer;
  winrt::com_ptr<ID2D1SolidColorBrush> mBackgroundBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mHighlightBrush;

  std::vector<NavigationEntry> mBookmarks;
  std::unordered_map<PageID, std::shared_ptr<LinkHandler>> mLinks;

  bool mNavigationLoaded = false;

  std::unordered_map<RenderTargetID, std::unique_ptr<CachedLayer>> mCache;
  std::unique_ptr<DoodleRenderer> mDoodles;
  std::shared_ptr<FilesystemWatcher> mWatcher;

  std::vector<PageID> mPageIDs;

  std::shared_mutex mMutex;
};

PDFFilePageSource::PDFFilePageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : p(new Impl {.mDXR = dxr}) {
  const std::unique_lock d2dlock(*(p->mDXR));
  p->mPDFRenderer = dxr->mPDFRenderer;
  p->mBackgroundBrush = dxr->mWhiteBrush;
  p->mHighlightBrush = dxr->mHighlightBrush;
  p->mDoodles = std::make_unique<DoodleRenderer>(dxr, kbs);
  AddEventListener(
    p->mDoodles->evAddedPageEvent, this->evAvailableFeaturesChangedEvent);
}

PDFFilePageSource::~PDFFilePageSource() {
  this->RemoveAllEventListeners();
  if (p->mPDFDocument) {
    // Windows.Data.Pdf.PdfDocument's destructor re-enters the message loop -
    // which means we can do:
    // 1. Enter message loop
    // 2. Enter ~PDFFilePageSource()
    // 3. Enter ~PdfDocument()
    // 4. Re-enter the message loop while `this` is partially destructed, and
    //    has an invalid vtable
    // 5. Double-free if the condition that led to the deletion is still
    //    present, or make pure virtual calls, or...

    // Work around this by rescheduling the deletion until 'later'
    [](auto threadGuard, auto deleteLater) -> winrt::fire_and_forget {
      threadGuard.CheckThread();
      // WIL::resume_foreground guarantees that we will be rescheduled, even
      // though we're not changing threads.
      co_await wil::resume_foreground(winrt::Microsoft::UI::Dispatching::
                                        DispatcherQueue::GetForCurrentThread());
    }(mThreadGuard, std::move(p->mPDFDocument));
  }
}

std::shared_ptr<PDFFilePageSource> PDFFilePageSource::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path) {
  auto ret = shared_with_final_release(new PDFFilePageSource(dxr, kbs));

  // Not a constructor argument as we need:
  // 1) async loading functions
  // 2) a weak_ptr to pass into the async loaders to ensure this object still
  //    exists when those finish executing
  // 3) so, we need the shared_ptr before we call `SetPath()`...
  // 4) so, we can't call `SetPath()` or `Reload()` from the constructor
  if (!path.empty()) {
    ret->SetPath(path);
  }

  return ret;
}

winrt::fire_and_forget PDFFilePageSource::ReloadRenderer() {
  auto weak = weak_from_this();
  auto uiThread = mUIThread;

  {
    auto self = weak.lock();
    if (!self) {
      co_return;
    }

    std::filesystem::path path;
    {
      std::shared_lock readLock(p->mMutex);
      path = p->mCopy->GetPath();

      if (!std::filesystem::is_regular_file(path)) {
        co_return;
      }
    }

    StorageFile file {nullptr};
    PdfDocument document {nullptr};
    try {
      // Windows 11 print-to-pdf tends to quickly create then delete/expand
      // PDFs, which give us a race condition
      //
      // Deal with it by catching these exceptions
      file = co_await StorageFile::GetFileFromPathAsync(path.wstring());
      document = co_await PdfDocument::LoadFromFileAsync(file);
    } catch (const winrt::hresult_error& e) {
      auto what = e.message();
      dprintf(
        "Failed to open {} for render: {}",
        path.string(),
        winrt::to_string(what));
      co_return;
    }
    dprintf("Opened PDF file {} for render", path.string());

    {
      std::unique_lock lock(p->mMutex);
      p->mPDFDocument = std::move(document);
      p->mPageIDs.resize(p->mPDFDocument.PageCount());
    }
  }

  co_await uiThread;
  if (auto self = weak.lock()) {
    evContentChangedEvent.Emit();
  }
}

winrt::fire_and_forget PDFFilePageSource::ReloadNavigation() {
  auto uiThread = mUIThread;
  auto weak = weak_from_this();

  if (!std::filesystem::is_regular_file(p->mCopy->GetPath())) {
    co_return;
  }

  co_await winrt::resume_background();
  auto stayingAlive = weak.lock();
  if (!stayingAlive) {
    co_return;
  }

  std::filesystem::path path;
  {
    std::shared_lock lock(p->mMutex);
    path = p->mCopy->GetPath();
  }
  PDFNavigation::PDF pdf(path);
  const auto bookmarks = pdf.GetBookmarks();
  decltype(p->mBookmarks) navigation;
  for (int i = 0; i < bookmarks.size(); i++) {
    const auto& it = bookmarks[i];
    navigation.push_back({it.mName, this->GetPageIDForIndex(it.mPageIndex)});
  }

  {
    std::unique_lock lock(p->mMutex);
    p->mBookmarks = std::move(navigation);
    p->mNavigationLoaded = true;
  }

  const auto links = pdf.GetLinks();
  decltype(p->mLinks) linkHandlers;
  for (int i = 0; i < links.size(); ++i) {
    const auto& pageLinks = links.at(i);
    auto handler = Impl::LinkHandler::Create(pageLinks);
    AddEventListener(
      handler->evClicked,
      [weak](EventContext ctx, const PDFNavigation::Link& link) {
        auto self = weak.lock();
        if (!self) {
          return;
        }
        const auto& dest = link.mDestination;
        switch (dest.mType) {
          case PDFNavigation::DestinationType::Page:
            self->evPageChangeRequestedEvent.Emit(
              ctx, self->GetPageIDForIndex(dest.mPageIndex));
            break;
          case PDFNavigation::DestinationType::URI: {
            [=]() -> winrt::fire_and_forget {
              co_await LaunchURI(dest.mURI);
            }();
            break;
          }
        }
      });
    linkHandlers[this->GetPageIDForIndex(i)] = handler;
  }

  {
    std::unique_lock lock(p->mMutex);
    p->mLinks = std::move(linkHandlers);
  }

  stayingAlive.reset();
  co_await uiThread;
  if (stayingAlive = weak.lock()) {
    this->evAvailableFeaturesChangedEvent.Emit();
  }
}

PageID PDFFilePageSource::GetPageIDForIndex(PageIndex index) const {
  if (!p) {
    return {};
  }
  {
    std::shared_lock lock(p->mMutex);
    if (index < p->mPageIDs.size()) {
      return p->mPageIDs.at(index);
    }
  }
  std::unique_lock lock(p->mMutex);
  p->mPageIDs.resize(index + 1);
  return p->mPageIDs.back();
}

winrt::fire_and_forget PDFFilePageSource::Reload() {
  static uint64_t sCount = 0;
  auto uiThread = mUIThread;
  auto weak = weak_from_this();

  {
    auto self = weak.lock();
    if (!self) {
      co_return;
    }

    std::unique_lock lock(p->mMutex);
    p->mCopy = {};
    p->mBookmarks.clear();
    p->mLinks.clear();
    p->mNavigationLoaded = false;
    p->mCache.clear();
    p->mPageIDs.clear();

    if (!std::filesystem::is_regular_file(p->mPath)) {
      co_return;
    }
  }

  // Do copy in a background thread so we're not hung up on antivirus
  co_await winrt::resume_background();

  auto self = weak.lock();
  if (!self) {
    co_return;
  }
  auto tempPath = Filesystem::GetTemporaryDirectory()
    / std::format(L"{:08x}-{}{}",
                  ++sCount,
                  p->mPath.stem().wstring().substr(0, 16),
                  p->mPath.extension());
  p->mCopy = std::make_shared<Filesystem::TemporaryCopy>(p->mPath, tempPath);

  this->ReloadRenderer();
  this->ReloadNavigation();
}

winrt::fire_and_forget PDFFilePageSource::final_release(
  std::unique_ptr<PDFFilePageSource> self) {
  co_await self->mUIThread;
  self->mThreadGuard.CheckThread();
}

PageIndex PDFFilePageSource::GetPageCount() const {
  std::shared_lock lock(p->mMutex);
  if (p->mPDFDocument) {
    return p->mPDFDocument.PageCount();
  }
  return 0;
}

std::vector<PageID> PDFFilePageSource::GetPageIDs() const {
  const auto pageCount = this->GetPageCount();
  if (pageCount == 0) {
    return {};
  }
  {
    std::shared_lock lock(p->mMutex);
    if (pageCount == p->mPageIDs.size()) {
      return p->mPageIDs;
    }
  }

  std::unique_lock lock(p->mMutex);
  p->mPageIDs.resize(pageCount);
  return p->mPageIDs;
}

PreferredSize PDFFilePageSource::GetPreferredSize(PageID id) {
  if (!p) {
    return {};
  }

  auto it = std::ranges::find(p->mPageIDs, id);
  if (it == p->mPageIDs.end()) {
    return {};
  }
  const auto index = it - p->mPageIDs.begin();
  auto size = p->mPDFDocument.GetPage(index).Size();

  return {
    {static_cast<UINT32>(size.Width), static_cast<UINT32>(size.Height)},
    ScalingKind::Vector,
  };
}

void PDFFilePageSource::RenderPageContent(
  RenderTarget* rt,
  PageID id,
  const D2D1_RECT_F& rect) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("PDFFilePageSource::RenderPageContent()");
  // Keep alive
  auto p = this->p;
  if (!p) {
    return;
  }

  std::shared_lock lock(p->mMutex);

  const auto pageIt = std::ranges::find(p->mPageIDs, id);
  if (pageIt == p->mPageIDs.end()) {
    return;
  }
  const auto index = pageIt - p->mPageIDs.begin();

  auto page = p->mPDFDocument.GetPage(index);

  auto ctx = rt->d2d();
  ctx->FillRectangle(rect, p->mBackgroundBrush.get());

  PDF_RENDER_PARAMS params {
    .DestinationWidth = static_cast<UINT>(rect.right - rect.left) + 1,
    .DestinationHeight = static_cast<UINT>(rect.bottom - rect.top) + 1,
  };

  ctx->SetTransform(D2D1::Matrix3x2F::Translation({rect.left, rect.top}));

  {
    const std::unique_lock d2dlock(*(p->mDXR));
    winrt::check_hresult(p->mPDFRenderer->RenderPageToDeviceContext(
      winrt::get_unknown(page), ctx, &params));
  }
}

void PDFFilePageSource::PostCursorEvent(
  EventContext ctx,
  const CursorEvent& ev,
  PageID pageID) {
  const auto contentSize = this->GetPreferredSize(pageID).mPixelSize;

  if (!p->mLinks.contains(pageID)) {
    p->mDoodles->PostCursorEvent(ctx, ev, pageID, contentSize);
    return;
  }

  auto links = p->mLinks.at(pageID);
  if (!links) {
    return;
  }

  scope_guard repaint([&]() { evNeedsRepaintEvent.Emit(); });

  CursorEvent pageEvent {ev};
  pageEvent.mX /= contentSize.mWidth;
  pageEvent.mY /= contentSize.mHeight;

  links->PostCursorEvent(ctx, pageEvent);

  if (links->HaveHoverOrPendingClick()) {
    return;
  }

  p->mDoodles->PostCursorEvent(ctx, ev, pageID, contentSize);
}

bool PDFFilePageSource::CanClearUserInput(PageID id) const {
  return p->mDoodles->HaveDoodles(id);
}

bool PDFFilePageSource::CanClearUserInput() const {
  return p->mDoodles->HaveDoodles();
}

void PDFFilePageSource::ClearUserInput(PageID id) {
  p->mDoodles->ClearPage(id);
}

void PDFFilePageSource::ClearUserInput() {
  p->mDoodles->Clear();
}

void PDFFilePageSource::RenderOverDoodles(
  ID2D1DeviceContext* ctx,
  PageID pageID,
  const D2D1_RECT_F& contentRect) {
  if (!p->mLinks.contains(pageID)) {
    return;
  }
  const auto hoverButton = p->mLinks.at(pageID)->GetHoverButton();
  if (!hoverButton) {
    return;
  }

  const auto contentWidth = contentRect.right - contentRect.left;
  const auto contentHeight = contentRect.bottom - contentRect.top;

  const D2D1_RECT_F rect {
    (hoverButton->mRect.left * contentWidth) + contentRect.left,
    (hoverButton->mRect.top * contentHeight) + contentRect.top,
    (hoverButton->mRect.right * contentWidth) + contentRect.left,
    (hoverButton->mRect.bottom * contentHeight) + contentRect.top,
  };
  const auto radius = contentHeight * 0.006f;
  ctx->DrawRoundedRectangle(
    D2D1::RoundedRect(rect, radius, radius),
    p->mHighlightBrush.get(),
    radius / 3);
}

std::filesystem::path PDFFilePageSource::GetPath() const {
  return p->mPath;
}

void PDFFilePageSource::SetPath(const std::filesystem::path& path) {
  if (path == p->mPath) {
    return;
  }
  p->mPath = path;
  p->mWatcher = FilesystemWatcher::Create(path);
  AddEventListener(
    p->mWatcher->evFilesystemModifiedEvent,
    [weak = weak_from_this()](auto path) {
      if (auto self = weak.lock()) {
        self->OnFileModified(path);
      }
    });
  Reload();
}

bool PDFFilePageSource::IsNavigationAvailable() const {
  return p->mNavigationLoaded && this->GetPageCount() > 2;
}

std::vector<NavigationEntry> PDFFilePageSource::GetNavigationEntries() const {
  std::shared_lock lock(p->mMutex);
  if (!p->mBookmarks.empty()) {
    return p->mBookmarks;
  }

  std::vector<NavigationEntry> entries;
  for (PageIndex i = 0; i < this->GetPageCount(); ++i) {
    entries.push_back({
      std::format(_("Page {} ({})"), i + 1, to_utf8(p->mPath.stem())),
      this->GetPageIDForIndex(i),
    });
  }
  return entries;
}

void PDFFilePageSource::RenderPage(
  RenderTarget* rt,
  PageID pageID,
  const D2D1_RECT_F& rect) {
  const auto rtid = rt->GetID();
  OPENKNEEBOARD_TraceLoggingScope("PDFFilePageSource::RenderPage()");
  if (!p->mCache.contains(rtid)) {
    p->mCache[rtid] = std::make_unique<CachedLayer>(p->mDXR);
  }
  p->mCache[rtid]->Render(
    rect, pageID.GetTemporaryValue(), rt, [=](auto rt, const auto& size) {
      this->RenderPageContent(
        rt,
        pageID,
        {
          0.0f,
          0.0f,
          static_cast<FLOAT>(size.width),
          static_cast<FLOAT>(size.height),
        });
    });

  const auto d2d = rt->d2d();
  p->mDoodles->Render(d2d, pageID, rect);
  this->RenderOverDoodles(d2d, pageID, rect);
}

void PDFFilePageSource::OnFileModified(const std::filesystem::path& path) {
  if (p && path == p->mPath) {
    this->Reload();
  }
}

}// namespace OpenKneeboard
