// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/CachedLayer.hpp>
#include <OpenKneeboard/CursorClickableRegions.hpp>
#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/DoodleRenderer.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/FilesystemWatcher.hpp>
#include <OpenKneeboard/LaunchURI.hpp>
#include <OpenKneeboard/NavigationTab.hpp>
#include <OpenKneeboard/PDFFilePageSource.hpp>
#include <OpenKneeboard/PDFNavigation.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/final_release_deleter.hpp>
#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <shims/nlohmann/json.hpp>
#include <shims/winrt/base.h>

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Data.Pdf.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <wil/cppwinrt.h>

#include <windows.data.pdf.interop.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <unordered_map>

#include <inttypes.h>

#include <wil/cppwinrt_helpers.h>

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
  static OpenKneeboard::fire_and_forget final_release(
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

task<std::shared_ptr<PDFFilePageSource>> PDFFilePageSource::Create(
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
    co_await ret->SetPath(path);
  }

  co_return ret;
}

task<void> PDFFilePageSource::ReloadRenderer(
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
      dprint(
        "Failed to open {} for render: {}",
        path.string(),
        winrt::to_string(what));
      co_return;
    }
    dprint("Opened PDF file {} for render", path.string());

    {
      const auto lock = wrap_lock(std::unique_lock {mMutex});
      // Another workaround for
      // https://github.com/microsoft/WindowsAppSDK/issues/3506
      if (doc->mPDFDocument) {
        ([](auto dq, auto deleteLater) -> OpenKneeboard::fire_and_forget {
          co_await wil::resume_foreground(dq);
        })(mUIThreadDispatcherQueue, std::move(doc->mPDFDocument));
      }
      doc->mPDFDocument = std::move(document);
      doc->mPageIDs.resize(doc->mPDFDocument.PageCount());
    }
  }

  evContentChangedEvent.Emit();
}

task<void> PDFFilePageSource::ReloadNavigation(
  std::weak_ptr<DocumentResources> weakDoc) {
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
    dprint("Failed to load PDFNavigation for PDF {}: {}", path, e.what());
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
      {
        weak,
        [](auto self, KneeboardViewID ctx, PDFNavigation::Link link)
          -> OpenKneeboard::fire_and_forget {
          const auto& dest = link.mDestination;
          switch (dest.mType) {
            case PDFNavigation::DestinationType::Page:
              self->evPageChangeRequestedEvent.Emit(
                ctx, self->GetPageIDForIndex(dest.mPageIndex));
              break;
            case PDFNavigation::DestinationType::URI: {
              co_await LaunchURI(dest.mURI);
              break;
            }
          }
        },
      });
    linkHandlers[this->GetPageIDForIndex(i)] = handler;
  }

  {
    const auto lock = wrap_lock(std::unique_lock {mMutex});
    doc->mLinks = std::move(linkHandlers);
  }

  this->evAvailableFeaturesChangedEvent.EnqueueForContext(mUIThread);
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

task<void> PDFFilePageSource::Reload() try {
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

  for (auto&& it: std::array<task<void>, 2> {
         this->ReloadRenderer(weakDoc), this->ReloadNavigation(weakDoc)}) {
    co_await std::move(it);
  }
} catch (const std::exception& e) {
  dprint.Warning("Exception reloading PDFFilePageSource: {}", e.what());
  OPENKNEEBOARD_BREAK;
} catch (const winrt::hresult_error& e) {
  dprint.Warning(
    "HRESULT Exception reloading PDFFilePageSource: {}",
    winrt::to_string(e.message()));
  OPENKNEEBOARD_BREAK;
}

OpenKneeboard::fire_and_forget PDFFilePageSource::final_release(
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

std::optional<PreferredSize> PDFFilePageSource::GetPreferredSize(PageID id) {
  if (!mDocumentResources) {
    return std::nullopt;
  }

  auto it = std::ranges::find(mDocumentResources->mPageIDs, id);
  if (it == mDocumentResources->mPageIDs.end()) {
    return std::nullopt;
  }
  const auto index = it - mDocumentResources->mPageIDs.begin();
  auto size = mDocumentResources->mPDFDocument.GetPage(index).Size();

  return PreferredSize {
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
  KneeboardViewID ctx,
  const CursorEvent& ev,
  PageID pageID) {
  const auto contentSize = this->GetPreferredSize(pageID);
  if (!contentSize) {
    return;
  }
  const auto& pixelSize = contentSize->mPixelSize;

  if (!mDocumentResources->mLinks.contains(pageID)) {
    mDoodles->PostCursorEvent(ctx, ev, pageID, pixelSize);
    return;
  }

  auto links = mDocumentResources->mLinks.at(pageID);
  if (!links) {
    return;
  }

  scope_exit repaint([&]() { evNeedsRepaintEvent.Emit(); });

  CursorEvent pageEvent {ev};
  pageEvent.mX /= pixelSize.mWidth;
  pageEvent.mY /= pixelSize.mHeight;

  links->PostCursorEvent(ctx, pageEvent);

  if (links->HaveHoverOrPendingClick()) {
    return;
  }

  mDoodles->PostCursorEvent(ctx, ev, pageID, pixelSize);
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

task<void> PDFFilePageSource::SetPath(const std::filesystem::path& path) {
  if (mDocumentResources && path == mDocumentResources->mPath) {
    co_return;
  }

  mDocumentResources
    = DocumentResources::Create(path, FilesystemWatcher::Create(path));

  AddEventListener(
    mDocumentResources->mWatcher->evFilesystemModifiedEvent,
    &PDFFilePageSource::OnFileModified | bind_refs_front(this));
  co_await this->Reload();
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

task<void>
PDFFilePageSource::RenderPage(RenderContext rc, PageID pageID, PixelRect rect) {
  auto rt = rc.GetRenderTarget();
  const auto rtid = rt->GetID();
  OPENKNEEBOARD_TraceLoggingScope("PDFFilePageSource::RenderPage()");
  if (!mDocumentResources->mCache.contains(rtid)) {
    mDocumentResources->mCache[rtid] = std::make_unique<CachedLayer>(mDXR);
  }

  const auto preferredSize = this->GetPreferredSize(pageID);
  if (!preferredSize) {
    co_return;
  }

  const auto cacheDimensions
    = preferredSize->mPixelSize.IntegerScaledToFit(MaxViewRenderSize);
  co_await mDocumentResources->mCache[rtid]->Render(
    rect,
    pageID.GetTemporaryValue(),
    rt,
    [](auto self, auto pageID, auto rt, auto size) -> task<void> {
      self->RenderPageContent(rt, pageID, {{0, 0}, size});
      co_return;
    } | bindline::bind_front(this, pageID),
    cacheDimensions);

  const auto d2d = rt->d2d();
  mDoodles->Render(d2d, pageID, rect);
  this->RenderOverDoodles(d2d, pageID, rect);
}

fire_and_forget PDFFilePageSource::OnFileModified(
  const std::filesystem::path& path) {
  if (mDocumentResources && path == mDocumentResources->mPath) {
    auto keepAlive = shared_from_this();
    co_await this->Reload();
  }
}
}// namespace OpenKneeboard