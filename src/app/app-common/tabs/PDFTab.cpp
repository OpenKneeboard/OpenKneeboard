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
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/NavigationTab.h>
#include <OpenKneeboard/PDFTab.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/utf8.h>
#include <inttypes.h>
#include <shims/winrt.h>
#include <windows.data.pdf.interop.h>
#include <winrt/windows.data.pdf.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.storage.h>

#include <chrono>
#include <nlohmann/json.hpp>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <thread>

using namespace winrt::Windows::Data::Pdf;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;

namespace {

struct InternalLink {
  QPDFObjectHandle::Rectangle mRect;
  QPDFObjGen mDestinationPage;
};

struct PageData {
  uint16_t mPageIndex;
  QPDFObjectHandle::Rectangle mRect;
  std::vector<InternalLink> mInternalLinks;
};

struct NormalizedLink {
  uint16_t mDestinationPageIndex;
  D2D1_RECT_F mRect;
};

enum class CursorLinkState {
  OUTSIDE_HYPERLINK,
  IN_HYPERLINK,
  PRESSED_IN_HYPERLINK,
  PRESSED_OUTSIDE_HYPERLINK,
};

}// namespace

namespace OpenKneeboard {
struct PDFTab::Impl final {
  DXResources mDXR;
  std::filesystem::path mPath;

  IPdfDocument mPDFDocument;
  winrt::com_ptr<IPdfRendererNative> mPDFRenderer;
  winrt::com_ptr<ID2D1SolidColorBrush> mBackgroundBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mHighlightBrush;

  std::vector<NavigationTab::Entry> mBookmarks;
  std::unordered_map<uint16_t, std::vector<NormalizedLink>> mLinks;

  CursorLinkState mLinkState = CursorLinkState::OUTSIDE_HYPERLINK;
  NormalizedLink mActiveLink;
  uint16_t mActivePage;
  bool mNavigationLoaded = false;
};

PDFTab::PDFTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view /* title */,
  const std::filesystem::path& path)
  : TabWithDoodles(dxr, kbs), p(new Impl {.mDXR = dxr, .mPath = path}) {
  winrt::check_hresult(
    PdfCreateRenderer(dxr.mDXGIDevice.get(), p->mPDFRenderer.put()));
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), p->mBackgroundBrush.put()));
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.8f, 1.0f, 1.0f), p->mHighlightBrush.put()));

  Reload();
}

PDFTab::PDFTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view title,
  const nlohmann::json& settings)
  : PDFTab(dxr, kbs, title, settings.at("Path").get<std::filesystem::path>()) {
}

PDFTab::~PDFTab() {
}

nlohmann::json PDFTab::GetSettings() const {
  return {{"Path", GetPath()}};
}

utf8_string PDFTab::GetGlyph() const {
  return "\uEA90";
}

utf8_string PDFTab::GetTitle() const {
  return p->mPath.stem();
}

namespace {

std::map<QPDFObjGen, PageData> FindAllHyperlinks(QPDF& pdf) {
  std::map<QPDFObjGen, PageData> pageMap;
  QPDFPageDocumentHelper pdh(pdf);
  QPDFOutlineDocumentHelper odh(pdf);

  uint16_t pageNumber = 0;
  for (auto& page: pdh.getAllPages()) {
    // Useful references:
    // - i7j-rups
    // -
    // https://www.adobe.com/content/dam/acom/en/devnet/pdf/pdfs/PDF32000_2008.pdf
    std::vector<InternalLink> links;
    for (auto& annotation: page.getAnnotations("/Link")) {
      auto aoh = annotation.getObjectHandle();

      if (aoh.hasKey("/Dest")) {
        auto dest = aoh.getKey("/Dest");
        auto destPage = dest.getArrayItem(0).getObjGen();
        auto linkRect = annotation.getRect();
        links.push_back({
          annotation.getRect(),
          destPage,
        });
        continue;
      }

      if (aoh.hasKey("/A")) {// action
        auto action = aoh.getKey("/A");
        if (!action.hasKey("/S")) {
          continue;
        }
        auto type = action.getKey("/S").getName();
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
        auto destPage = dest.getArrayItem(0).getObjGen();
        auto linkRect = annotation.getRect();
        links.push_back({
          annotation.getRect(),
          destPage,
        });
        continue;
      }
    }

    pageMap.emplace(
      page.getObjectHandle().getObjGen(),
      PageData {pageNumber++, page.getTrimBox().getArrayAsRectangle(), links});
  }

  return pageMap;
}

std::vector<NavigationTab::Entry> GetNavigationEntries(
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
    auto kidEntries = GetNavigationEntries(pageIndices, kids);
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

void PDFTab::Reload() {
  p->mBookmarks.clear();
  p->mLinkState = CursorLinkState::OUTSIDE_HYPERLINK;
  p->mActiveLink = {};
  p->mActivePage = ~(0ui16);
  p->mNavigationLoaded = false;

  if (!std::filesystem::is_regular_file(p->mPath)) {
    return;
  }
  std::thread {[this] {
    SetThreadDescription(GetCurrentThread(), L"PDFTab PdfDocument Thread");
    auto file = StorageFile::GetFileFromPathAsync(p->mPath.wstring()).get();
    p->mPDFDocument = PdfDocument::LoadFromFileAsync(file).get();
    this->evFullyReplacedEvent.EmitFromMainThread();
  }}.detach();
  std::thread {[this] {
    SetThreadDescription(GetCurrentThread(), L"PDFTab QPDF Thread");
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
        pageIndices[page.getObjectHandle().getObjGen()] = nextIndex++;
      }
    }

    QPDFOutlineDocumentHelper odh(qpdf);
    auto outlines = odh.getTopLevelOutlines();
    p->mBookmarks = GetNavigationEntries(pageIndices, outlines);
    if (p->mBookmarks.empty()) {
      p->mBookmarks.clear();
      const auto pageCount = QPDFPageDocumentHelper(qpdf).getAllPages().size();
      for (uint16_t i = 0; i < pageCount; ++i) {
        p->mBookmarks.push_back({std::format(_("Page {}"), i + 1), i});
      }
    }
    p->mNavigationLoaded = true;

    const auto outlineTime
      = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);
    dprintf("QPDF outline time: {}ms", outlineTime);

    this->evAvailableFeaturesChangedEvent.EmitFromMainThread();

    auto pageHyperlinkData = FindAllHyperlinks(qpdf);
    for (const auto& [_handle, page]: pageHyperlinkData) {
      if (page.mInternalLinks.empty()) {
        continue;
      }
      // PDF origin is bottom left, DirectX is top left
      const auto pageWidth
        = static_cast<float>(page.mRect.urx - page.mRect.llx);
      const auto pageHeight
        = static_cast<float>(page.mRect.ury - page.mRect.lly);

      std::vector<NormalizedLink> links;
      for (const auto& pdfLink: page.mInternalLinks) {
        if (!pageHyperlinkData.contains(pdfLink.mDestinationPage)) {
          continue;
        }
        // Convert coordinates here :)
        links.push_back(
          {pageHyperlinkData.at(pdfLink.mDestinationPage).mPageIndex,
           D2D1_RECT_F {
             .left = static_cast<float>(pdfLink.mRect.llx - page.mRect.llx)
               / pageWidth,
             .top = 1.0f
               - (static_cast<float>(pdfLink.mRect.ury - page.mRect.lly)
                  / pageHeight),
             .right = static_cast<float>(pdfLink.mRect.urx - pdfLink.mRect.llx)
               / pageWidth,
             .bottom = 1.0f
               - (static_cast<float>(pdfLink.mRect.lly - page.mRect.lly)
                  / pageHeight),
           }});
      }
      p->mLinks.emplace(page.mPageIndex, links);
    }
  }}.detach();
}

uint16_t PDFTab::GetPageCount() const {
  if (p->mPDFDocument) {
    return p->mPDFDocument.PageCount();
  }
  return 0;
}

D2D1_SIZE_U PDFTab::GetNativeContentSize(uint16_t index) {
  if (index >= GetPageCount()) {
    return {};
  }
  auto size = p->mPDFDocument.GetPage(index).Size();
  // scale to fit to get higher quality text rendering
  const auto scaleX = TextureWidth / size.Width;
  const auto scaleY = TextureHeight / size.Height;
  const auto scale = std::min(scaleX, scaleY);
  size.Width *= scale;
  size.Height *= scale;

  return {static_cast<UINT32>(size.Width), static_cast<UINT32>(size.Height)};
}

void PDFTab::RenderPageContent(
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

void PDFTab::PostCursorEvent(
  EventContext ctx,
  const CursorEvent& ev,
  uint16_t pageIndex) {
  if (pageIndex != p->mActivePage) {
    p->mActiveLink = {};
    p->mLinkState = CursorLinkState::OUTSIDE_HYPERLINK;
    p->mActivePage = pageIndex;
  }
  if (!p->mLinks.contains(pageIndex)) {
    p->mLinkState = CursorLinkState::OUTSIDE_HYPERLINK;
    TabWithDoodles::PostCursorEvent(ctx, ev, pageIndex);
    return;
  }
  const auto contentRect = this->GetNativeContentSize(pageIndex);
  const auto x = ev.mX / contentRect.width;
  const auto y = ev.mY / contentRect.height;

  bool haveHoverLink = false;
  NormalizedLink hoverLink;
  for (const auto& link: p->mLinks.at(pageIndex)) {
    const auto& linkRect = link.mRect;
    if (
      x >= linkRect.left && x <= linkRect.right && y >= linkRect.top
      && y <= linkRect.bottom) {
      haveHoverLink = true;
      hoverLink = link;
      break;
    }
  }

  if (ev.mTouchState != CursorTouchState::TOUCHING_SURFACE) {
    if (
      p->mLinkState == CursorLinkState::PRESSED_IN_HYPERLINK
      && x >= p->mActiveLink.mRect.left && x <= p->mActiveLink.mRect.right
      && y >= p->mActiveLink.mRect.top && y <= p->mActiveLink.mRect.bottom) {
      this->evPageChangeRequestedEvent.Emit(
        ctx, p->mActiveLink.mDestinationPageIndex);
    }

    if (haveHoverLink) {
      p->mActiveLink = hoverLink;
      p->mLinkState = CursorLinkState::IN_HYPERLINK;
    } else {
      p->mLinkState = CursorLinkState::OUTSIDE_HYPERLINK;
    }
    TabWithDoodles::PostCursorEvent(ctx, ev, pageIndex);
    evNeedsRepaintEvent.Emit();
    return;
  }

  // Touching surface

  if (p->mLinkState == CursorLinkState::PRESSED_IN_HYPERLINK) {
    return;
  }

  if (p->mLinkState == CursorLinkState::PRESSED_OUTSIDE_HYPERLINK) {
    TabWithDoodles::PostCursorEvent(ctx, ev, pageIndex);
    return;
  }

  if (haveHoverLink) {
    p->mActiveLink = hoverLink;
    p->mLinkState = CursorLinkState::PRESSED_IN_HYPERLINK;
    evNeedsRepaintEvent.Emit();
  } else {
    p->mLinkState = CursorLinkState::PRESSED_OUTSIDE_HYPERLINK;
    TabWithDoodles::PostCursorEvent(ctx, ev, pageIndex);
  }
}

void PDFTab::RenderOverDoodles(
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
  const D2D1_RECT_F& contentRect) {
  if (pageIndex != p->mActivePage) {
    return;
  }
  if (
    p->mLinkState == CursorLinkState::OUTSIDE_HYPERLINK
    || p->mLinkState == CursorLinkState::PRESSED_OUTSIDE_HYPERLINK) {
    return;
  }

  const auto contentWidth = contentRect.right - contentRect.left;
  const auto contentHeight = contentRect.bottom - contentRect.top;

  const D2D1_RECT_F rect {
    (p->mActiveLink.mRect.left * contentWidth) + contentRect.left,
    (p->mActiveLink.mRect.top * contentHeight) + contentRect.top,
    (p->mActiveLink.mRect.right * contentWidth) + contentRect.left,
    (p->mActiveLink.mRect.bottom * contentHeight) + contentRect.top,
  };
  const auto radius = contentHeight * 0.006f;
  ctx->DrawRoundedRectangle(
    D2D1::RoundedRect(rect, radius, radius),
    p->mHighlightBrush.get(),
    radius / 3);
}

std::filesystem::path PDFTab::GetPath() const {
  return p->mPath;
}

void PDFTab::SetPath(const std::filesystem::path& path) {
  if (path == p->mPath) {
    return;
  }
  p->mPath = path;
  Reload();
}

bool PDFTab::IsNavigationAvailable() const {
  return p->mNavigationLoaded && this->GetPageCount() > 2;
}

std::shared_ptr<Tab> PDFTab::CreateNavigationTab(uint16_t pageIndex) {
  return std::make_shared<NavigationTab>(
    p->mDXR, this, p->mBookmarks, this->GetNativeContentSize(pageIndex));
}

}// namespace OpenKneeboard
