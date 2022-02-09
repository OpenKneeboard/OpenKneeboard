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
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/NavigationTab.h>
#include <OpenKneeboard/PDFTab.h>
#include <OpenKneeboard/dprint.h>
#include <shims/winrt.h>
#include <windows.data.pdf.interop.h>
#include <winrt/windows.data.pdf.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.storage.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <thread>

#include <inttypes.h>

using namespace winrt::Windows::Data::Pdf;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;

namespace OpenKneeboard {
struct PDFTab::Impl final {
  DXResources mDXR;
  std::filesystem::path mPath;

  IPdfDocument mPDFDocument;
  winrt::com_ptr<IPdfRendererNative> mPDFRenderer;
  winrt::com_ptr<ID2D1SolidColorBrush> mBackgroundBrush;

  std::vector<NavigationTab::Entry> mBookmarks;
};

PDFTab::PDFTab(
  const DXResources& dxr,
  const wxString& title,
  const std::filesystem::path& path)
  : TabWithDoodles(dxr), p(new Impl {.mDXR = dxr, .mPath = path}) {
  winrt::check_hresult(
    PdfCreateRenderer(dxr.mDXGIDevice.get(), p->mPDFRenderer.put()));
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), p->mBackgroundBrush.put()));

  Reload();
}

PDFTab::~PDFTab() {
}

std::wstring PDFTab::GetTitle() const {
  return p->mPath.stem().wstring();
}

namespace {

std::map<QPDFObjGen, uint16_t> GetPageMap(QPDF& pdf) {
  std::map<QPDFObjGen, uint16_t> pageMap;
  QPDFPageDocumentHelper pdh(pdf);

  uint16_t pageNumber = 0;
  for (auto& page: pdh.getAllPages()) {
    auto object = page.getObjectHandle();
    auto gen = object.getObjGen();
    pageMap.emplace(page.getObjectHandle().getObjGen(), pageNumber++);
  }

  return pageMap;
}

std::vector<NavigationTab::Entry> GetNavigationEntries(
  std::map<QPDFObjGen, uint16_t>& pageMap,
  std::vector<QPDFOutlineObjectHelper>& outlines) {
  std::vector<NavigationTab::Entry> entries;

  for (auto& outline: outlines) {
    auto page = outline.getDestPage();
    auto key = page.getObjGen();
    if (!pageMap.contains(key)) {
      continue;
    }
    entries.push_back({
      .mName = std::wstring(winrt::to_hstring(outline.getTitle())),
      .mPageIndex = pageMap.at(key),
    });

    auto kids = outline.getKids();
    auto kidEntries = GetNavigationEntries(pageMap, kids);
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
  std::thread([=]() {
    auto file = StorageFile::GetFileFromPathAsync(p->mPath.wstring()).get();
    p->mPDFDocument = PdfDocument::LoadFromFileAsync(file).get();
    evFullyReplacedEvent();
  }).detach();

  const auto pathStr = p->mPath.string();
  QPDF qpdf;
  qpdf.processFile(pathStr.c_str());
  QPDFOutlineDocumentHelper odh(qpdf);
  if (!odh.hasOutlines()) {
    return;
  }

  auto pageMap = GetPageMap(qpdf);
  auto outlines = odh.getTopLevelOutlines();
  p->mBookmarks = GetNavigationEntries(pageMap, outlines);
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
  return;
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
  return p->mBookmarks.size() > 1;
}

std::shared_ptr<Tab> PDFTab::CreateNavigationTab(uint16_t pageIndex) {
  return std::make_shared<NavigationTab>(
    p->mDXR, this, p->mBookmarks, this->GetNativeContentSize(pageIndex));
}

}// namespace OpenKneeboard
