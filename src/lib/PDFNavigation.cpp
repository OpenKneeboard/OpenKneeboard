// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include <OpenKneeboard/DebugTimer.hpp>
#include <OpenKneeboard/PDFNavigation.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>

namespace OpenKneeboard::PDFNavigation {

using PageIndexMap = std::map<QPDFObjGen, PageIndex>;

struct PDF::Impl {
  Impl(const std::filesystem::path&);
  ~Impl();

  QPDF mQPDF;
  std::optional<QPDFOutlineDocumentHelper> mOutlineDocumentHelper;
  std::vector<QPDFPageObjectHelper> mPages;
  PageIndexMap mPageIndices;
  winrt::file_handle mFile;
  winrt::handle mMapping;
  void* mView = nullptr;

  Impl() = delete;
  Impl(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl& operator=(Impl&&) = delete;
};

PDF::PDF(const std::filesystem::path& path) : p(new Impl(path)) {
}

PDF::PDF(PDF&&) = default;
PDF::~PDF() = default;

PDF& PDF::operator=(PDF&&) = default;

static QPDFObjectHandle PageFromDest(
  QPDFOutlineDocumentHelper& outlineHelper,
  QPDFObjectHandle dest) {
  if (dest.isString() || dest.isName()) {
    dest = outlineHelper.resolveNamedDest(dest);
  }

  if (dest.isDictionary() && dest.hasKey("/D")) {
    dest = dest.getKey("/D");
  }

  if (!dest.isArray()) {
    return {};
  }
  if (dest.getArrayNItems() >= 1) {
    auto first = dest.getArrayItem(0);
    if (first.isPageObject()) {
      return first;
    }
  }
  return {};
}

static void ExtractBookmarks(
  QPDFOutlineDocumentHelper& outlineHelper,
  std::vector<QPDFOutlineObjectHelper>& outlines,
  const PageIndexMap& pageIndices,
  std::back_insert_iterator<std::vector<Bookmark>> inserter) noexcept {
  // Useful references:
  // - i7j-rups
  // -
  // https://www.adobe.com/content/dam/acom/en/devnet/pdf/pdfs/PDF32000_2008.pdf
  for (auto& outline: outlines) {
    auto page = outline.getDestPage();
    if (page.isNull()) {
      page = PageFromDest(outlineHelper, outline.getDest());
    }
    auto key = page.getObjGen();
    if (!pageIndices.contains(key)) {
      continue;
    }
    inserter = {
      .mName = outline.getTitle(),
      .mPageIndex = pageIndices.at(key),
    };

    auto kids = outline.getKids();
    ExtractBookmarks(outlineHelper, kids, pageIndices, inserter);
  }
}

static std::vector<Bookmark> ExtractBookmarks(
  QPDFOutlineDocumentHelper& outlineHelper,
  const PageIndexMap& pageIndices) noexcept {
  std::vector<Bookmark> bookmarks;
  DebugTimer timer("Bookmarks");
  auto outlines = outlineHelper.getTopLevelOutlines();
  ExtractBookmarks(
    outlineHelper, outlines, pageIndices, std::back_inserter(bookmarks));
  return bookmarks;
}

static bool PushDestLink(
  QPDFOutlineDocumentHelper& outlineHelper,
  QPDFObjectHandle& it,
  const PageIndexMap& pageIndices,
  const D2D1_RECT_F& rect,
  std::vector<Link>& links) {
  if (!it.hasKey("/Dest")) {
    return false;
  }

  auto page = PageFromDest(outlineHelper, it.getKey("/Dest"));
  const auto destRef = page.getObjGen();
  if (!pageIndices.contains(destRef)) {
    return false;
  }
  auto destPage = pageIndices.at(destRef);
  links.push_back({
    rect,
    {
      .mType = DestinationType::Page,
      .mPageIndex = destPage,
    },
  });
  return true;
}

static void PushURIActionLink(
  QPDFObjectHandle& action,
  const D2D1_RECT_F& rect,
  std::vector<Link>& links) {
  if (!action.hasKey("/URI")) {
    return;
  }

  auto uri = action.getKey("/URI").getStringValue();
  links.push_back({
    rect,
    {
      .mType = DestinationType::URI,
      .mURI = uri,
    },
  });
  return;
}

static void PushGoToActionLink(
  QPDFOutlineDocumentHelper& outlineHelper,
  const PageIndexMap& pageIndices,
  QPDFObjectHandle& action,
  const D2D1_RECT_F& rect,
  std::vector<Link>& links) {
  if (!action.hasKey("/D")) {
    return;
  }

  auto dest = PageFromDest(outlineHelper, action.getKey("/D"));
  const auto pageRef = dest.getObjGen();
  if (!pageIndices.contains(pageRef)) {
    return;
  }
  auto destPage = pageIndices.at(pageRef);
  links.push_back({
    rect,
    {
      .mType = DestinationType::Page,
      .mPageIndex = destPage,
    },
  });
}

static bool PushActionLink(
  QPDFOutlineDocumentHelper& outlineHelper,
  const PageIndexMap& pageIndices,
  QPDFObjectHandle& it,
  const D2D1_RECT_F& rect,
  std::vector<Link>& links) {
  if (!it.hasKey("/A")) {
    return false;
  }
  auto action = it.getKey("/A");
  if (!action.hasKey("/S")) {
    return true;
  }

  auto type = action.getKey("/S").getName();

  if (type == "/URI") {
    PushURIActionLink(action, rect, links);
    return true;
  }

  if (type == "/GoTo") {
    PushGoToActionLink(outlineHelper, pageIndices, action, rect, links);
    return true;
  }

  return true;
}

static std::vector<Link> ExtractLinks(
  QPDFOutlineDocumentHelper& outlineHelper,
  QPDFPageObjectHelper& page,
  const PageIndexMap& pageIndices) {
  auto annotations = page.getAnnotations("/Link");
  if (annotations.empty()) {
    return {};
  }

  auto pageRect = page.getCropBox().getArrayAsRectangle();
  const auto pageWidth = static_cast<float>(pageRect.urx - pageRect.llx);
  const auto pageHeight = static_cast<float>(pageRect.ury - pageRect.lly);

  std::vector<Link> links;
  for (auto& annotation: annotations) {
    // Convert bottom-left origin (PDF) to top-left origin (everything else,
    // including D2D)
    const auto pdfRect = annotation.getRect();
    const D2D1_RECT_F linkRect {
      .left = static_cast<float>(pdfRect.llx - pageRect.llx) / pageWidth,
      .top
      = 1.0f - (static_cast<float>(pdfRect.ury - pageRect.lly) / pageHeight),
      .right = static_cast<float>(pdfRect.urx - pageRect.llx) / pageWidth,
      .bottom
      = 1.0f - (static_cast<float>(pdfRect.lly - pageRect.lly) / pageHeight),
    };

    auto handle = annotation.getObjectHandle();
    if (PushDestLink(outlineHelper, handle, pageIndices, linkRect, links)) {
      continue;
    }
    if (PushActionLink(outlineHelper, pageIndices, handle, linkRect, links)) {
      continue;
    }
  }
  return links;
}

static std::vector<std::vector<Link>> ExtractLinks(
  QPDFOutlineDocumentHelper& outlineHelper,
  std::vector<QPDFPageObjectHelper>& pages,
  const PageIndexMap& pageIndices) {
  DebugTimer timer("Links");
  std::vector<std::vector<Link>> allLinks;
  allLinks.reserve(pages.size());

  for (auto& page: pages) {
    allLinks.push_back(ExtractLinks(outlineHelper, page, pageIndices));
  }

  return allLinks;
}

PDF::Impl::Impl(const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(path)) {
    dprint(L"Can't find PDF file {}", path.wstring());
    return;
  }

  DebugTimer initTimer("PDF Init");

  const auto fileSize = std::filesystem::file_size(path);
  const auto wpath = path.wstring();
  mFile = Win32::or_default::CreateFile(
    wpath.c_str(),
    GENERIC_READ,
    FILE_SHARE_READ,
    nullptr,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL);
  if (!mFile) {
    dprint("Failed to open PDF with CreateFileW");
    return;
  }
  mMapping = Win32::or_default::CreateFileMapping(
    mFile.get(),
    nullptr,
    PAGE_READONLY,
    fileSize >> 32,
    static_cast<DWORD>(fileSize),
    nullptr);
  if (!mMapping) {
    dprint("Failed to create file mapping of PDF");
    return;
  }
  mView = MapViewOfFile(mMapping.get(), FILE_MAP_READ, 0, 0, fileSize);

  const auto utf8Path = to_utf8(path);
  mQPDF.processMemoryFile(
    utf8Path.c_str(), reinterpret_cast<const char*>(mView), fileSize);
  mPages = QPDFPageDocumentHelper(mQPDF).getAllPages();
  for (const auto& page: mPages) {
    mPageIndices[page.getObjectHandle().getObjGen()] = mPageIndices.size();
  }
  mOutlineDocumentHelper.emplace(mQPDF);
}

PDF::Impl::~Impl() {
  if (mView) {
    UnmapViewOfFile(mView);
  }
}

std::vector<Bookmark> PDF::GetBookmarks() {
  if (p->mPages.empty()) {
    return {};
  }
  return ExtractBookmarks(*p->mOutlineDocumentHelper, p->mPageIndices);
}

std::vector<std::vector<Link>> PDF::GetLinks() {
  if (p->mPages.empty()) {
    return {};
  }
  return ExtractLinks(*p->mOutlineDocumentHelper, p->mPages, p->mPageIndices);
}

}// namespace OpenKneeboard::PDFNavigation
