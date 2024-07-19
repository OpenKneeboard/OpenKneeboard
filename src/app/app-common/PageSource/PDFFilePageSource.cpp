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
#include <unordered_map>

#include <inttypes.h>

using namespace winrt::Windows::Data::Pdf;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;

namespace OpenKneeboard {

// Convenience wrapper to make it easy to wrap all locks in `DPrintLifetime`
static constexpr auto wrap_lock(
  auto&& lock,
  [[maybe_unused]] const std::source_location& location
  = std::source_location::current()) {
  if constexpr (false) {
    return DPrintLifetime<std::decay_t<decltype(lock)>> {
      "PDFLock", std::move(lock), location};
  } else {
    return lock;
  }
}

struct PDFFilePageSource::DocumentResources final {
  using LinkHandler = CursorClickableRegions<PDFNavigation::Link>;

  std::filesystem::path mPath;
  std::shared_ptr<Filesystem::TemporaryCopy> mCopy;

  std::shared_ptr<FilesystemWatcher> mWatcher;

  PdfDocument mPDFDocument {nullptr};

  std::vector<NavigationEntry> mBookmarks;
  std::unordered_map<PageID, std::shared_ptr<LinkHandler>> mLinks;

  bool mNavigationLoaded = false;

  std::unordered_map<RenderTargetID, std::unique_ptr<CachedLayer>> mCache;

  std::vector<PageID> mPageIDs;

  static auto Create(
    const std::filesystem::path& path,
    std::shared_ptr<FilesystemWatcher>&& watcher) {
    return std::shared_ptr<DocumentResources> {
      new DocumentResources(path, std::move(watcher)),
      final_release_deleter<DocumentResources> {}};
  }

  /** Work around https://github.com/microsoft/WindowsAppSDK/issues/3506
   *
   * Windows.Data.Pdf.PdfDocument's destructor re-enters the message loop -
   * which means we can do:
   * 1. Enter message loop
   * 2. Enter ~PDFFilePageSource()
   * 3. Enter ~PdfDocument()
   * 4. Re-enter the message loop while `this` is partially destructed, and
   *    has an invalid vtable
   * 5. Double-free if the condition that led to the deletion is still
   *    present, or make pure virtual calls, or...
   */
  static winrt::fire_and_forget final_release(
    std::unique_ptr<DocumentResources> self) {
    co_await wil::resume_foreground(self->mDispatcherQueue);
    self->mPDFDocument = nullptr;
    if (!self->mCopy) {
      co_return;
    }
    // Leave cleaning up the copy until the renderer is done
    co_await wil::resume_foreground(self->mDispatcherQueue);
  }

  DocumentResources() = delete;

 private:
  DocumentResources(
    const std::filesystem::path& path,
    std::shared_ptr<FilesystemWatcher>&& watcher)
    : mPath(path), mWatcher(std::move(watcher)) {
  }

  using DispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue;
  DispatcherQueue mDispatcherQueue = DispatcherQueue::GetForCurrentThread();
};

PDFFilePageSource::PDFFilePageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
  : mDXR(dxr) {
  const std::unique_lock d2dlock(*mDXR);
  mBackgroundBrush = dxr->mWhiteBrush;
  mHighlightBrush = dxr->mHighlightBrush;
  mDoodles = std::make_unique<DoodleRenderer>(dxr, kbs);
  AddEventListener(
    mDoodles->evAddedPageEvent, this->evAvailableFeaturesChangedEvent);
}

PDFFilePageSource::~PDFFilePageSource() {
  this->RemoveAllEventListeners();
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

winrt::fire_and_forget PDFFilePageSource::ReloadRenderer(
  std::weak_ptr<DocumentResources> weakDoc) {
  auto weak = weak_from_this();

  {
    auto self = weak.lock();
    auto doc = weakDoc.lock();
    if (!(self && doc && doc == self->mDocumentResources)) {
      co_return;
    }

    std::filesystem::path path;
    {
      const auto readLock = wrap_lock(std::shared_lock {mMutex});
      if (!(doc && doc->mCopy)) {
        co_return;
      }
      path = doc->mCopy->GetPath();

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
      const auto lock = wrap_lock(std::unique_lock {mMutex});
      // Another workaround for
      // https://github.com/microsoft/WindowsAppSDK/issues/3506
      if (doc->mPDFDocument) {
        ([](auto dq, auto deleteLater) -> winrt::fire_and_forget {
          co_await wil::resume_foreground(dq);
        })(mUIThreadDispatcherQueue, std::move(doc->mPDFDocument));
      }
      doc->mPDFDocument = std::move(document);
      doc->mPageIDs.resize(doc->mPDFDocument.PageCount());
    }
  }

  auto uiThread = mUIThread;
  co_await uiThread;

  if (auto self = weak.lock()) {
    evContentChangedEvent.Emit();
  }
}

winrt::fire_and_forget PDFFilePageSource::ReloadNavigation(
  std::weak_ptr<DocumentResources> weakDoc) {
  auto uiThread = mUIThread;
  auto weak = weak_from_this();

  {
    auto doc = weakDoc.lock();
    if (!(doc && doc->mCopy)) {
      co_return;
    }

    if (!std::filesystem::is_regular_file(doc->mCopy->GetPath())) {
      co_return;
    }
  }

  co_await winrt::resume_background();
  auto stayingAlive = weak.lock();
  auto doc = weakDoc.lock();
  if (!(stayingAlive && doc && doc == mDocumentResources)) {
    co_return;
  }

  std::filesystem::path path;
  {
    const auto lock = wrap_lock(std::shared_lock {mMutex});
    path = doc->mCopy->GetPath();
  }
  std::optional<PDFNavigation::PDF> maybePdf;
  try {
    maybePdf.emplace(path);
  } catch (const std::runtime_error& e) {
    dprintf("Failed to load PDFNavigation for PDF {}: {}", path, e.what());
    co_return;
  }
  auto pdf = std::move(*maybePdf);

  const auto bookmarks = pdf.GetBookmarks();
  decltype(doc->mBookmarks) navigation;
  for (int i = 0; i < bookmarks.size(); i++) {
    const auto& it = bookmarks[i];
    navigation.push_back({it.mName, this->GetPageIDForIndex(it.mPageIndex)});
  }

  {
    const auto lock = wrap_lock(std::unique_lock {mMutex});
    doc->mBookmarks = std::move(navigation);
    doc->mNavigationLoaded = true;
  }

  const auto links = pdf.GetLinks();
  decltype(mDocumentResources->mLinks) linkHandlers;
  for (int i = 0; i < links.size(); ++i) {
    const auto& pageLinks = links.at(i);
    auto handler = DocumentResources::LinkHandler::Create(pageLinks);
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
    const auto lock = wrap_lock(std::unique_lock {mMutex});
    doc->mLinks = std::move(linkHandlers);
  }

  stayingAlive.reset();
  co_await uiThread;
  if (stayingAlive = weak.lock()) {
    this->evAvailableFeaturesChangedEvent.Emit();
  }
}

PageID PDFFilePageSource::GetPageIDForIndex(PageIndex index) const {
  {
    const auto lock = wrap_lock(std::shared_lock {mMutex});
    if (!mDocumentResources) {
      return {};
    }
    if (index < mDocumentResources->mPageIDs.size()) {
      return mDocumentResources->mPageIDs.at(index);
    }
  }
  const auto lock = wrap_lock(std::unique_lock {mMutex});
  if (!mDocumentResources) {
    return {};
  }
  mDocumentResources->mPageIDs.resize(index + 1);
  TraceLoggingWrite(
    gTraceProvider,
    "PDFFilePageSource::GetPageIDForIndex()",
    TraceLoggingValue(index, "Index"),
    TraceLoggingHexUInt64(
      mDocumentResources->mPageIDs.back().GetTemporaryValue(), "PageID"));
  return mDocumentResources->mPageIDs.back();
}

winrt::fire_and_forget PDFFilePageSource::Reload() try {
  static uint64_t sCount = 0;
  auto uiThread = mUIThread;
  auto weak = weak_from_this();

  std::weak_ptr<DocumentResources> weakDoc;

  {
    auto self = weak.lock();
    if (!(self && mDocumentResources)) {
      co_return;
    }

    mDoodles->Clear();

    const auto lock = wrap_lock(std::unique_lock {mMutex});

    mDocumentResources = DocumentResources::Create(
      mDocumentResources->mPath, std::move(mDocumentResources->mWatcher));

    if (!std::filesystem::is_regular_file(mDocumentResources->mPath)) {
      co_return;
    }

    weakDoc = mDocumentResources;
  }

  // Do copy in a background thread so we're not hung up on antivirus
  co_await winrt::resume_background();

  auto self = weak.lock();
  {
    auto doc = weakDoc.lock();
    if (!(self && doc)) {
      co_return;
    }
    auto tempPath = Filesystem::GetTemporaryDirectory()
      / std::format(L"{:08x}-{}{}",
                    ++sCount,
                    doc->mPath.stem().wstring().substr(0, 16),
                    doc->mPath.extension());
    doc->mCopy
      = std::make_shared<Filesystem::TemporaryCopy>(doc->mPath, tempPath);
  }

  co_await uiThread;

  this->ReloadRenderer(weakDoc);
  this->ReloadNavigation(weakDoc);

} catch (const std::exception& e) {
  dprintf("WARNING: Exception reloading PDFFilePageSource: {}", e.what());
  OPENKNEEBOARD_BREAK;
}

winrt::fire_and_forget PDFFilePageSource::final_release(
  std::unique_ptr<PDFFilePageSource> self) {
  co_await self->mUIThread;
  self->mThreadGuard.CheckThread();
}

PageIndex PDFFilePageSource::GetPageCount() const {
  const auto lock = wrap_lock(std::shared_lock {mMutex});
  if (mDocumentResources && mDocumentResources->mPDFDocument) {
    return mDocumentResources->mPDFDocument.PageCount();
  }
  return 0;
}

std::vector<PageID> PDFFilePageSource::GetPageIDs() const {
  const auto pageCount = this->GetPageCount();
  if (pageCount == 0) {
    return {};
  }
  {
    const auto lock = wrap_lock(std::shared_lock {mMutex});
    if (pageCount == mDocumentResources->mPageIDs.size()) {
      return mDocumentResources->mPageIDs;
    }
  }

  const auto lock = wrap_lock(std::unique_lock {mMutex});
  mDocumentResources->mPageIDs.resize(pageCount);

  if (TraceLoggingProviderEnabled(gTraceProvider, 0, 0)) {
    std::vector<uint64_t> values(pageCount);
    std::ranges::transform(
      mDocumentResources->mPageIDs, begin(values), &PageID::GetTemporaryValue);
    TraceLoggingWrite(
      gTraceProvider,
      "PDFFilePageSource::GetPageIDs()",
      TraceLoggingHexUInt64Array(values.data(), values.size(), "PageIDs"));
  }

  return mDocumentResources->mPageIDs;
}

PreferredSize PDFFilePageSource::GetPreferredSize(PageID id) {
  if (!mDocumentResources) {
    return {};
  }

  auto it = std::ranges::find(mDocumentResources->mPageIDs, id);
  if (it == mDocumentResources->mPageIDs.end()) {
    return {};
  }
  const auto index = it - mDocumentResources->mPageIDs.begin();
  auto size = mDocumentResources->mPDFDocument.GetPage(index).Size();

  return {
    {static_cast<UINT32>(size.Width), static_cast<UINT32>(size.Height)},
    ScalingKind::Vector,
  };
}

void PDFFilePageSource::RenderPageContent(
  RenderTarget* rt,
  PageID id,
  const PixelRect& rect) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("PDFFilePageSource::RenderPageContent()");
  // Keep alive
  auto doc = this->mDocumentResources;
  if (!doc) {
    return;
  }

  const auto lock = wrap_lock(std::shared_lock {mMutex});

  const auto pageIt = std::ranges::find(doc->mPageIDs, id);
  if (pageIt == doc->mPageIDs.end()) {
    return;
  }
  const auto index = pageIt - doc->mPageIDs.begin();

  auto page = doc->mPDFDocument.GetPage(index);

  auto ctx = rt->d2d();
  ctx->FillRectangle(rect, mBackgroundBrush.get());

  PDF_RENDER_PARAMS params {
    .DestinationWidth = rect.Width(),
    .DestinationHeight = rect.Height(),
  };

  ctx->SetTransform(D2D1::Matrix3x2F::Translation(
    rect.TopLeft().StaticCast<FLOAT, D2D1_SIZE_F>()));

  {
    const std::unique_lock d2dlock(*mDXR);
    winrt::check_hresult(mDXR->mPDFRenderer->RenderPageToDeviceContext(
      winrt::get_unknown(page), ctx, &params));
  }
}

void PDFFilePageSource::PostCursorEvent(
  EventContext ctx,
  const CursorEvent& ev,
  PageID pageID) {
  const auto contentSize = this->GetPreferredSize(pageID).mPixelSize;

  if (!mDocumentResources->mLinks.contains(pageID)) {
    mDoodles->PostCursorEvent(ctx, ev, pageID, contentSize);
    return;
  }

  auto links = mDocumentResources->mLinks.at(pageID);
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

  mDoodles->PostCursorEvent(ctx, ev, pageID, contentSize);
}

bool PDFFilePageSource::CanClearUserInput(PageID id) const {
  return mDoodles->HaveDoodles(id);
}

bool PDFFilePageSource::CanClearUserInput() const {
  return mDoodles->HaveDoodles();
}

void PDFFilePageSource::ClearUserInput(PageID id) {
  mDoodles->ClearPage(id);
}

void PDFFilePageSource::ClearUserInput() {
  mDoodles->Clear();
}

void PDFFilePageSource::RenderOverDoodles(
  ID2D1DeviceContext* ctx,
  PageID pageID,
  const D2D1_RECT_F& contentRect) {
  const auto lock = wrap_lock(std::shared_lock(mMutex));
  if (!mDocumentResources) {
    return;
  }

  if (!mDocumentResources->mLinks.contains(pageID)) {
    return;
  }
  const auto hoverButton
    = mDocumentResources->mLinks.at(pageID)->GetHoverButton();
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
    D2D1::RoundedRect(rect, radius, radius), mHighlightBrush.get(), radius / 3);
}

std::filesystem::path PDFFilePageSource::GetPath() const {
  return mDocumentResources->mPath;
}

void PDFFilePageSource::SetPath(const std::filesystem::path& path) {
  if (mDocumentResources && path == mDocumentResources->mPath) {
    return;
  }

  mDocumentResources
    = DocumentResources::Create(path, FilesystemWatcher::Create(path));

  AddEventListener(
    mDocumentResources->mWatcher->evFilesystemModifiedEvent,
    [weak = weak_from_this()](auto path) {
      if (auto self = weak.lock()) {
        self->OnFileModified(path);
      }
    });
  Reload();
}

bool PDFFilePageSource::IsNavigationAvailable() const {
  return mDocumentResources->mNavigationLoaded && this->GetPageCount() > 2;
}

std::vector<NavigationEntry> PDFFilePageSource::GetNavigationEntries() const {
  const auto lock = wrap_lock(std::shared_lock {mMutex});
  if (!mDocumentResources->mBookmarks.empty()) {
    return mDocumentResources->mBookmarks;
  }

  std::vector<NavigationEntry> entries;
  for (PageIndex i = 0; i < this->GetPageCount(); ++i) {
    entries.push_back({
      std::format(
        _("Page {} ({})"), i + 1, to_utf8(mDocumentResources->mPath.stem())),
      this->GetPageIDForIndex(i),
    });
  }
  return entries;
}

void PDFFilePageSource::RenderPage(
  RenderTarget* rt,
  PageID pageID,
  const PixelRect& rect) {
  const auto rtid = rt->GetID();
  OPENKNEEBOARD_TraceLoggingScope("PDFFilePageSource::RenderPage()");
  if (!mDocumentResources->mCache.contains(rtid)) {
    mDocumentResources->mCache[rtid] = std::make_unique<CachedLayer>(mDXR);
  }
  const auto cacheDimensions
    = this->GetPreferredSize(pageID).mPixelSize.IntegerScaledToFit(
      MaxViewRenderSize);
  mDocumentResources->mCache[rtid]->Render(
    rect,
    pageID.GetTemporaryValue(),
    rt,
    [=](auto rt, const auto& size) {
      this->RenderPageContent(rt, pageID, {{0, 0}, size});
    },
    cacheDimensions);

  const auto d2d = rt->d2d();
  mDoodles->Render(d2d, pageID, rect);
  this->RenderOverDoodles(d2d, pageID, rect);
}

void PDFFilePageSource::OnFileModified(const std::filesystem::path& path) {
  if (mDocumentResources && path == mDocumentResources->mPath) {
    this->Reload();
  }
}

}// namespace OpenKneeboard
