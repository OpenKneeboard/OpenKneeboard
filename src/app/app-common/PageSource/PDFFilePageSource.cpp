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
#include <OpenKneeboard/LaunchURI.h>
#include <OpenKneeboard/NavigationTab.h>
#include <OpenKneeboard/PDFFilePageSource.h>
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

#include <chrono>
#include <cstring>
#include <nlohmann/json.hpp>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <thread>

using namespace winrt::Windows::Data::Pdf;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;

namespace {

struct LinkDestination {
  enum class Type {
    Page,
    URI,
  };
  Type mType;
  uint16_t mPageIndex;
  std::string mURI;
};

static bool operator==(const D2D1_RECT_F& a, const D2D1_RECT_F& b) noexcept {
  return std::memcmp(&a, &b, sizeof(a)) == 0;
}

struct NormalizedLink {
  D2D1_RECT_F mRect;
  LinkDestination mDestination;

  bool operator==(const NormalizedLink& other) const noexcept {
    return mRect == other.mRect;
  }
};

}// namespace

namespace OpenKneeboard {

struct PDFFilePageSource::Impl final {
  using LinkHandler = CursorClickableRegions<NormalizedLink>;

  DXResources mDXR;
  std::filesystem::path mPath;

  IPdfDocument mPDFDocument;
  winrt::com_ptr<IPdfRendererNative> mPDFRenderer;
  winrt::com_ptr<ID2D1SolidColorBrush> mBackgroundBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mHighlightBrush;

  std::vector<NavigationTab::Entry> mBookmarks;
  std::vector<std::shared_ptr<LinkHandler>> mLinks;

  bool mNavigationLoaded = false;
  std::jthread mQPDFThread;

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
  if (p && p->mQPDFThread.get_id() != std::thread::id {}) {
    p->mQPDFThread.request_stop();
    p->mQPDFThread.join();
  }
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

namespace {

std::vector<std::vector<NormalizedLink>> FindAllHyperlinks(
  QPDF& pdf,
  std::stop_token stopToken) {
  QPDFPageDocumentHelper pdh(pdf);
  auto pages = pdh.getAllPages();

  std::map<QPDFObjGen, uint16_t> pageNumbers;
  uint16_t pageIndex = 0;
  for (auto& page: pages) {
    pageNumbers.emplace(page.getObjectHandle().getObjGen(), pageIndex++);
  }

  QPDFOutlineDocumentHelper odh(pdf);

  std::vector<std::vector<NormalizedLink>> ret;
  for (auto& page: pages) {
    if (stopToken.stop_requested()) {
      return {};
    }
    // Useful references:
    // - i7j-rups
    // -
    // https://www.adobe.com/content/dam/acom/en/devnet/pdf/pdfs/PDF32000_2008.pdf
    std::vector<NormalizedLink> links;
    auto pageRect = page.getCropBox().getArrayAsRectangle();
    const auto pageWidth = static_cast<float>(pageRect.urx - pageRect.llx);
    const auto pageHeight = static_cast<float>(pageRect.ury - pageRect.lly);
    for (auto& annotation: page.getAnnotations("/Link")) {
      auto aoh = annotation.getObjectHandle();

      // Convert bottom-left origin (PDF) to top-left origin (everything else,
      // including D2D)
      const auto pdfRect = annotation.getRect();
      const D2D1_RECT_F rect {
        .left = static_cast<float>(pdfRect.llx - pageRect.llx) / pageWidth,
        .top
        = 1.0f - (static_cast<float>(pdfRect.ury - pageRect.lly) / pageHeight),
        .right = static_cast<float>(pdfRect.urx - pageRect.llx) / pageWidth,
        .bottom
        = 1.0f - (static_cast<float>(pdfRect.lly - pageRect.lly) / pageHeight),
      };

      if (aoh.hasKey("/Dest")) {
        auto dest = aoh.getKey("/Dest");
        auto destRef = dest.getArrayItem(0).getObjGen();
        if (!pageNumbers.contains(destRef)) {
          continue;
        }
        auto destPage = pageNumbers.at(destRef);
        auto linkRect = annotation.getRect();
        links.push_back({
          rect,
          {.mType = LinkDestination::Type::Page, .mPageIndex = destPage},
        });
        continue;
      }

      if (aoh.hasKey("/A")) {// action
        auto action = aoh.getKey("/A");
        if (!action.hasKey("/S")) {
          continue;
        }
        auto type = action.getKey("/S").getName();

        if (type == "/URI") {
          if (!action.hasKey("/URI")) {
            continue;
          }
          auto uri = action.getKey("/URI").getStringValue();
          if (uri.starts_with("openkneeboard://")) {
            dprintf("Found magic URI in PDF: {}", uri);
          }
          links.push_back({
            rect,
            {
              .mType = LinkDestination::Type::URI,
              .mURI = uri,
            },
          });
          continue;
        }

        if (type != "/GoTo") {
          continue;
        }
        if (!action.hasKey("/D")) {
          continue;
        }
        auto dest = action.getKey("/D");
        if (!(dest.isName() || dest.isString())) {
          continue;
        }
        dest = odh.resolveNamedDest(dest);
        if (!dest.isArray()) {
          continue;
        }
        auto destRef = dest.getArrayItem(0).getObjGen();
        if (!pageNumbers.contains(destRef)) {
          continue;
        }
        auto destPage = pageNumbers.at(destRef);
        auto linkRect = annotation.getRect();
        links.push_back({
          rect,
          {
            .mType = LinkDestination::Type::Page,
            .mPageIndex = destPage,
          },
        });
        continue;
      }
    }

    ret.push_back(links);
  }

  return ret;
}

std::vector<NavigationTab::Entry> ExtractNavigationEntries(
  const std::map<QPDFObjGen, uint16_t>& pageIndices,
  std::vector<QPDFOutlineObjectHelper>& outlines) {
  std::vector<NavigationTab::Entry> entries;

  for (auto& outline: outlines) {
    auto page = outline.getDestPage();
    auto key = page.getObjGen();
    if (!pageIndices.contains(key)) {
      continue;
    }
    entries.push_back(NavigationTab::Entry {
      .mName = outline.getTitle(),
      .mPageIndex = pageIndices.at(key),
    });

    auto kids = outline.getKids();
    auto kidEntries = ExtractNavigationEntries(pageIndices, kids);
    if (kidEntries.empty()) {
      continue;
    }
    entries.reserve(entries.size() + kidEntries.size());
    std::copy(
      kidEntries.begin(), kidEntries.end(), std::back_inserter(entries));
  }

  return entries;
}

}// namespace

void PDFFilePageSource::Reload() {
  p->mBookmarks.clear();
  p->mLinks.clear();
  p->mNavigationLoaded = false;

  if (!std::filesystem::is_regular_file(p->mPath)) {
    return;
  }

  auto weakThis = this->weak_from_this();

  // captures are undefined after the first co_await
  [](auto weakThis) -> winrt::fire_and_forget {
    co_await winrt::resume_background();
    auto strongThis = weakThis.lock();
    if (!strongThis) {
      co_return;
    }

    auto file = co_await StorageFile::GetFileFromPathAsync(
      strongThis->p->mPath.wstring());
    strongThis->p->mPDFDocument = co_await PdfDocument::LoadFromFileAsync(file);
    strongThis->evContentChangedEvent.Emit(ContentChangeType::FullyReplaced);
  }(weakThis);

  p->mQPDFThread = {[weakThis, this](std::stop_token stopToken) {
    const auto stayingAlive = weakThis.lock();
    if (!stayingAlive) {
      return;
    }

    SetThreadDescription(GetCurrentThread(), L"PDFFilePageSource QPDF Thread");
    const auto startTime = std::chrono::steady_clock::now();
    scope_guard timer([startTime]() {
      const auto endTime = std::chrono::steady_clock::now();
      dprintf(
        "QPDF processing time: {}ms",
        std::chrono::duration_cast<std::chrono::milliseconds>(
          endTime - startTime));
    });

    const auto fileSize = std::filesystem::file_size(p->mPath);
    const auto wpath = p->mPath.wstring();
    winrt::handle file {CreateFileW(
      wpath.c_str(),
      GENERIC_READ,
      FILE_SHARE_READ,
      nullptr,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL,
      NULL)};
    if (!file) {
      dprint("Failed to open PDF with CreateFileW");
      return;
    }
    winrt::handle mapping {CreateFileMappingW(
      file.get(),
      nullptr,
      PAGE_READONLY,
      fileSize >> 32,
      static_cast<DWORD>(fileSize),
      nullptr)};
    if (!mapping) {
      dprint("Failed to create file mapping of PDF");
      return;
    }
    auto fileView = MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, fileSize);
    scope_guard fileViewGuard([fileView]() { UnmapViewOfFile(fileView); });

    QPDF qpdf;
    auto path = to_utf8(wpath);
    qpdf.processMemoryFile(
      path.c_str(), reinterpret_cast<const char*>(fileView), fileSize);

    // Page outlines/TOC
    std::map<QPDFObjGen, uint16_t> pageIndices;
    {
      uint16_t nextIndex = 0;
      for (const auto& page: QPDFPageDocumentHelper(qpdf).getAllPages()) {
        if (stopToken.stop_requested()) {
          return;
        }
        pageIndices[page.getObjectHandle().getObjGen()] = nextIndex++;
      }
    }

    QPDFOutlineDocumentHelper odh(qpdf);
    auto outlines = odh.getTopLevelOutlines();
    p->mBookmarks = ExtractNavigationEntries(pageIndices, outlines);
    if (p->mBookmarks.empty()) {
      p->mBookmarks.clear();
      for (uint16_t i = 0; i < pageIndices.size(); ++i) {
        p->mBookmarks.push_back({std::format(_("Page {}"), i + 1), i});
      }
    }
    p->mNavigationLoaded = true;

    const auto outlineTime
      = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);
    dprintf("QPDF outline time: {}ms", outlineTime);

    if (stopToken.stop_requested()) {
      return;
    }
    this->evAvailableFeaturesChangedEvent.Emit();

    const auto links = FindAllHyperlinks(qpdf, stopToken);
    for (const auto& pageLinks: links) {
      auto handler = std::make_shared<Impl::LinkHandler>(pageLinks);
      AddEventListener(
        handler->evClicked, [this](auto ctx, const NormalizedLink& link) {
          const auto& dest = link.mDestination;
          switch (dest.mType) {
            case LinkDestination::Type::Page:
              this->evPageChangeRequestedEvent.Emit(ctx, dest.mPageIndex);
              break;
            case LinkDestination::Type::URI: {
              [=]() -> winrt::fire_and_forget {
                co_await LaunchURI(dest.mURI);
              }();
              break;
            }
          }
        });
      p->mLinks.push_back(handler);
    }
  }};
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
