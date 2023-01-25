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
#include <OpenKneeboard/LaunchURI.h>
#include <OpenKneeboard/NavigationTab.h>
#include <OpenKneeboard/PDFFilePageSource.h>
#include <OpenKneeboard/PDFNavigation.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/utf8.h>
#include <inttypes.h>
#include <shims/winrt/base.h>
#include <windows.data.pdf.interop.h>
#include <winrt/Windows.Data.Pdf.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <cstring>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <random>
#include <thread>

using namespace winrt::Windows::Data::Pdf;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;

namespace OpenKneeboard {

struct PDFFilePageSource::Impl final {
  using LinkHandler = CursorClickableRegions<PDFNavigation::Link>;

  DXResources mDXR;
  std::filesystem::path mPath;
  std::unique_ptr<Filesystem::TemporaryCopy> mCopy;

  IPdfDocument mPDFDocument;
  winrt::com_ptr<IPdfRendererNative> mPDFRenderer;
  winrt::com_ptr<ID2D1SolidColorBrush> mBackgroundBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mHighlightBrush;

  std::vector<NavigationEntry> mBookmarks;
  std::vector<std::shared_ptr<LinkHandler>> mLinks;

  bool mNavigationLoaded = false;

  std::unique_ptr<CachedLayer> mContentLayer;
  std::unique_ptr<DoodleRenderer> mDoodles;
};

PDFFilePageSource::PDFFilePageSource(
  const DXResources& dxr,
  KneeboardState* kbs)
  : p(new Impl {.mDXR = dxr}) {
  const auto d2dLock = dxr.AcquireLock();
  winrt::check_hresult(
    PdfCreateRenderer(dxr.mDXGIDevice.get(), p->mPDFRenderer.put()));
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), p->mBackgroundBrush.put()));
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.8f, 1.0f, 1.0f), p->mHighlightBrush.put()));

  p->mContentLayer = std::make_unique<CachedLayer>(dxr);
  p->mDoodles = std::make_unique<DoodleRenderer>(dxr, kbs);
  AddEventListener(
    p->mDoodles->evAddedPageEvent, this->evAvailableFeaturesChangedEvent);
}

PDFFilePageSource::~PDFFilePageSource() {
  this->RemoveAllEventListeners();
}

std::shared_ptr<PDFFilePageSource> PDFFilePageSource::Create(
  const DXResources& dxr,
  KneeboardState* kbs,
  const std::filesystem::path& path) {
  auto ret
    = std::shared_ptr<PDFFilePageSource>(new PDFFilePageSource(dxr, kbs));

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
  auto weakThis = this->weak_from_this();
  co_await winrt::resume_background();
  auto strongThis = weakThis.lock();
  if (!strongThis) {
    co_return;
  }

  auto path = strongThis->p->mCopy->GetPath();

  if (!std::filesystem::is_regular_file(path)) {
    co_return;
  }

  auto file = co_await StorageFile::GetFileFromPathAsync(path.wstring());
  strongThis->p->mPDFDocument = co_await PdfDocument::LoadFromFileAsync(file);

  const EventDelay eventDelay;
  strongThis->evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
}

winrt::fire_and_forget PDFFilePageSource::ReloadNavigation() {
  auto weakThis = this->weak_from_this();

  co_await winrt::resume_background();
  auto stayingAlive = weakThis.lock();
  if (!stayingAlive) {
    co_return;
  }

  auto path = p->mCopy->GetPath();
  if (!std::filesystem::is_regular_file(path)) {
    co_return;
  }

  PDFNavigation::PDF pdf(path);
  for (const auto& it: pdf.GetBookmarks()) {
    p->mBookmarks.push_back({it.mName, it.mPageIndex});
  }
  p->mNavigationLoaded = true;
  {
    const EventDelay delay;
    this->evAvailableFeaturesChangedEvent.Emit();
  }

  for (const auto& pageLinks: pdf.GetLinks()) {
    auto handler = Impl::LinkHandler::Create(pageLinks);
    AddEventListener(
      handler->evClicked,
      [this](EventContext ctx, const PDFNavigation::Link& link) {
        const auto& dest = link.mDestination;
        switch (dest.mType) {
          case PDFNavigation::DestinationType::Page:
            this->evPageChangeRequestedEvent.Emit(ctx, dest.mPageIndex);
            break;
          case PDFNavigation::DestinationType::URI: {
            [=]() -> winrt::fire_and_forget {
              co_await LaunchURI(dest.mURI);
            }();
            break;
          }
        }
      });
    p->mLinks.push_back(handler);
  }
}

void PDFFilePageSource::Reload() {
  static uint64_t sCount = 0;

  p->mCopy = {};
  p->mBookmarks.clear();
  p->mLinks.clear();
  p->mNavigationLoaded = false;

  if (!std::filesystem::is_regular_file(p->mPath)) {
    return;
  }

  auto tempPath = Filesystem::GetTemporaryDirectory()
    / std::format(L"{:08x}-{}{}",
                  ++sCount,
                  p->mPath.stem().wstring().substr(0, 16),
                  p->mPath.extension());
  p->mCopy = std::make_unique<Filesystem::TemporaryCopy>(p->mPath, tempPath);

  this->ReloadRenderer();
  this->ReloadNavigation();
}

PageIndex PDFFilePageSource::GetPageCount() const {
  if (p->mPDFDocument) {
    return p->mPDFDocument.PageCount();
  }
  return 0;
}

D2D1_SIZE_U PDFFilePageSource::GetNativeContentSize(PageIndex index) {
  if (index >= GetPageCount()) {
    return {};
  }
  auto size = p->mPDFDocument.GetPage(index).Size();

  return {static_cast<UINT32>(size.Width), static_cast<UINT32>(size.Height)};
}

void PDFFilePageSource::RenderPageContent(
  ID2D1DeviceContext* ctx,
  PageIndex index,
  const D2D1_RECT_F& rect) {
  if (index >= GetPageCount()) {
    return;
  }

  auto page = p->mPDFDocument.GetPage(index);
  auto size = page.Size();

  ctx->FillRectangle(rect, p->mBackgroundBrush.get());

  PDF_RENDER_PARAMS params {
    .DestinationWidth = static_cast<UINT>(rect.right - rect.left) + 1,
    .DestinationHeight = static_cast<UINT>(rect.bottom - rect.top) + 1,
  };

  ctx->SetTransform(D2D1::Matrix3x2F::Translation({rect.left, rect.top}));
  scope_guard resetTransform(
    [ctx]() { ctx->SetTransform(D2D1::Matrix3x2F::Identity()); });

  {
    const auto d2dLock = p->mDXR.AcquireLock();
    winrt::check_hresult(p->mPDFRenderer->RenderPageToDeviceContext(
      winrt::get_unknown(page), ctx, &params));
  }

  // `RenderPageToDeviceContext()` starts a multi-threaded job, but needs
  // the `page` pointer to stay valid until it has finished - so, flush to
  // get everything in the direct2d queue done.
  winrt::check_hresult(ctx->Flush());
}

void PDFFilePageSource::PostCursorEvent(
  EventContext ctx,
  const CursorEvent& ev,
  PageIndex pageIndex) {
  const auto contentRect = this->GetNativeContentSize(pageIndex);

  if (pageIndex >= p->mLinks.size()) {
    p->mDoodles->PostCursorEvent(ctx, ev, pageIndex, contentRect);
    return;
  }

  auto links = p->mLinks.at(pageIndex);
  if (!links) {
    return;
  }

  scope_guard repaint([&]() { evNeedsRepaintEvent.Emit(); });

  CursorEvent pageEvent {ev};
  pageEvent.mX /= contentRect.width;
  pageEvent.mY /= contentRect.height;

  links->PostCursorEvent(ctx, pageEvent);

  if (links->HaveHoverOrPendingClick()) {
    return;
  }

  p->mDoodles->PostCursorEvent(ctx, ev, pageIndex, contentRect);
}

bool PDFFilePageSource::CanClearUserInput(PageIndex index) const {
  return p->mDoodles->HaveDoodles(index);
}

bool PDFFilePageSource::CanClearUserInput() const {
  return p->mDoodles->HaveDoodles();
}

void PDFFilePageSource::ClearUserInput(PageIndex pageIndex) {
  p->mDoodles->ClearPage(pageIndex);
}

void PDFFilePageSource::ClearUserInput() {
  p->mDoodles->Clear();
}

void PDFFilePageSource::RenderOverDoodles(
  ID2D1DeviceContext* ctx,
  PageIndex pageIndex,
  const D2D1_RECT_F& contentRect) {
  if (pageIndex >= p->mLinks.size()) {
    return;
  }
  const auto hoverButton = p->mLinks.at(pageIndex)->GetHoverButton();
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
  Reload();
}

bool PDFFilePageSource::IsNavigationAvailable() const {
  return p->mNavigationLoaded && this->GetPageCount() > 2;
}

std::vector<NavigationEntry> PDFFilePageSource::GetNavigationEntries() const {
  return p->mBookmarks;
}

void PDFFilePageSource::RenderPage(
  RenderTargetID,
  ID2D1DeviceContext* ctx,
  PageIndex pageIndex,
  const D2D1_RECT_F& rect) {
  const auto size = this->GetNativeContentSize(pageIndex);
  p->mContentLayer->Render(
    rect, size, pageIndex, ctx, [=](auto ctx, const auto& size) {
      this->RenderPageContent(
        ctx,
        pageIndex,
        {
          0.0f,
          0.0f,
          static_cast<FLOAT>(size.width),
          static_cast<FLOAT>(size.height),
        });
    });
  p->mDoodles->Render(ctx, pageIndex, rect);
  this->RenderOverDoodles(ctx, pageIndex, rect);
}

}// namespace OpenKneeboard
