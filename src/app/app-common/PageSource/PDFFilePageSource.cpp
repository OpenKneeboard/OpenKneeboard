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
#include <OpenKneeboard/PDFIPC.h>
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
  struct Link {
    D2D1_RECT_F mRect;
    PDFIPC::Destination mDestination;

    constexpr bool operator==(const Link& other) const noexcept {
      return mDestination == other.mDestination
        && memcmp(&mRect, &other.mRect, sizeof(mRect)) == 0;
    }
  };
  using LinkHandler = CursorClickableRegions<Link>;

  DXResources mDXR;
  std::filesystem::path mPath;

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
  winrt::check_hresult(
    PdfCreateRenderer(dxr.mDXGIDevice.get(), p->mPDFRenderer.put()));
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), p->mBackgroundBrush.put()));
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.8f, 1.0f, 1.0f), p->mHighlightBrush.put()));

  p->mContentLayer = std::make_unique<CachedLayer>(dxr);
  p->mDoodles = std::make_unique<DoodleRenderer>(dxr, kbs);
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

  if (!std::filesystem::is_regular_file(strongThis->p->mPath)) {
    co_return;
  }

  auto file = co_await StorageFile::GetFileFromPathAsync(
    strongThis->p->mPath.wstring());
  strongThis->p->mPDFDocument = co_await PdfDocument::LoadFromFileAsync(file);

  const EventDelay eventDelay;
  strongThis->evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
}

winrt::fire_and_forget PDFFilePageSource::ReloadNavigation() {
  auto weakThis = this->weak_from_this();
  auto strongThis = this->shared_from_this();

  if (!std::filesystem::is_regular_file(strongThis->p->mPath)) {
    co_return;
  }

  const auto tempDir = Filesystem::GetTemporaryDirectory();

  std::random_device randDevice;
  std::uniform_int_distribution<uint64_t> randDist;
  const auto randPart = randDist(randDevice);
  const auto pid = static_cast<uint32_t>(GetCurrentProcessId());

  const auto requestPath = tempDir
    / std::format("OpenKneeboard-{}-{:016x}-pdf-request.json", pid, randPart);
  const auto responsePath = tempDir
    / std::format("OpenKneeboard-{}-{:016x}-pdf-response.json", pid, randPart);

  const Filesystem::ScopedDeleter requestDeleter(requestPath);
  const Filesystem::ScopedDeleter responseDeleter(responsePath);

  {
    const PDFIPC::Request request {
      .mPDFFilePath = to_utf8(strongThis->p->mPath),
      .mOutputFilePath = to_utf8(responsePath),
    };

    std::ofstream(requestPath.wstring())
      << std::setw(2) << nlohmann::json(request) << std::endl;
  }

  {
    const auto helperPath
      = (Filesystem::GetRuntimeDirectory() / RuntimeFiles::PDF_HELPER)
          .wstring();
    std::wstring mutableCommandLine
      = std::format(L"{} {}", helperPath, requestPath.wstring());
    mutableCommandLine.resize(mutableCommandLine.size() + 1, L'\0');
    STARTUPINFOW startupInfo {.cb = sizeof(STARTUPINFOW)};
    PROCESS_INFORMATION processInfo {};
    if (!CreateProcessW(
          helperPath.c_str(),
          mutableCommandLine.data(),
          nullptr,
          nullptr,
          false,
          0,
          nullptr,
          nullptr,
          &startupInfo,
          &processInfo)) {
      dprintf(
        "PDFHelper CreateProcessW failed: {:#016x}",
        std::bit_cast<uint32_t>(GetLastError()));
      co_return;
    }
    winrt::handle processHandle {processInfo.hProcess};
    winrt::handle threadHandle {processInfo.hThread};

    co_await winrt::resume_on_signal(processHandle.get());
  }

  if (!std::filesystem::is_regular_file(responsePath)) {
    dprint("No response from PDF helper :'(");
    co_return;
  }

  const auto response
    = nlohmann::json::parse(std::ifstream(responsePath.wstring()))
        .get<PDFIPC::Response>();

  for (const auto& it: response.mBookmarks) {
    strongThis->p->mBookmarks.push_back({it.mName, it.mPageIndex});
  }
  strongThis->p->mNavigationLoaded = true;
  {
    const EventDelay delay;
    strongThis->evAvailableFeaturesChangedEvent.Emit();
  }

  for (const auto& ipcPageLinks: response.mLinksByPage) {
    std::vector<Impl::Link> pageLinks;
    pageLinks.reserve(ipcPageLinks.size());
    for (const auto& link: ipcPageLinks) {
      pageLinks.push_back(
        {{
           .left = link.mRect.mLeft,
           .top = link.mRect.mTop,
           .right = link.mRect.mRight,
           .bottom = link.mRect.mBottom,
         },
         link.mDestination});
    }

    auto handler = std::make_shared<Impl::LinkHandler>(pageLinks);
    strongThis->AddEventListener(
      handler->evClicked, [this](EventContext ctx, const Impl::Link& link) {
        const auto& dest = link.mDestination;
        switch (dest.mType) {
          case PDFIPC::DestinationType::Page:
            this->evPageChangeRequestedEvent.Emit(ctx, dest.mPageIndex);
            break;
          case PDFIPC::DestinationType::URI: {
            [=]() -> winrt::fire_and_forget {
              co_await LaunchURI(dest.mURI);
            }();
            break;
          }
        }
      });
    strongThis->p->mLinks.push_back(handler);
  }
}

void PDFFilePageSource::Reload() {
  p->mBookmarks.clear();
  p->mLinks.clear();
  p->mNavigationLoaded = false;

  if (!std::filesystem::is_regular_file(p->mPath)) {
    return;
  }

  this->ReloadRenderer();
  this->ReloadNavigation();
}

uint16_t PDFFilePageSource::GetPageCount() const {
  if (p->mPDFDocument) {
    return p->mPDFDocument.PageCount();
  }
  return 0;
}

D2D1_SIZE_U PDFFilePageSource::GetNativeContentSize(uint16_t index) {
  if (index >= GetPageCount()) {
    return {};
  }
  auto size = p->mPDFDocument.GetPage(index).Size();

  return {static_cast<UINT32>(size.Width), static_cast<UINT32>(size.Height)};
}

void PDFFilePageSource::RenderPageContent(
  ID2D1DeviceContext* ctx,
  uint16_t index,
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

  winrt::check_hresult(p->mPDFRenderer->RenderPageToDeviceContext(
    winrt::get_unknown(page), ctx, &params));

  // `RenderPageToDeviceContext()` starts a multi-threaded job, but needs
  // the `page` pointer to stay valid until it has finished - so, flush to
  // get everything in the direct2d queue done.
  winrt::check_hresult(ctx->Flush());
}

void PDFFilePageSource::PostCursorEvent(
  EventContext ctx,
  const CursorEvent& ev,
  uint16_t pageIndex) {
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

void PDFFilePageSource::RenderOverDoodles(
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
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
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
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
