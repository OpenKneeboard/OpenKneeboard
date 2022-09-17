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

/** PDFium does not support multithreading, and recommends multiprocessing
 * instead.
 *
 * Command line arguments: temporary file path.
 *
 * Input (in temporary file):
 *   JSON-encoded `OpenKneeboard::PDFIPC::Request`
 *
 * Output (in path indicated in input):
 *   JSON-encoded `OpenKneeboard::PDFIPC::Response`
 */

#include <OpenKneeboard/DebugTimer.h>
#include <OpenKneeboard/PDFIPC.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/utf8.h>
#include <Windows.h>
#include <shellapi.h>
#include <shims/winrt/base.h>

#include <fstream>
#include <map>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <shims/filesystem>

using namespace OpenKneeboard;
using namespace OpenKneeboard::PDFIPC;

using PageIndexMap = std::map<QPDFObjGen, uint16_t>;

static void ExtractBookmarks(
  std::vector<QPDFOutlineObjectHelper>& outlines,
  const PageIndexMap& pageIndices,
  std::back_insert_iterator<std::vector<Bookmark>> inserter) {
  // Useful references:
  // - i7j-rups
  // -
  // https://www.adobe.com/content/dam/acom/en/devnet/pdf/pdfs/PDF32000_2008.pdf
  for (auto& outline: outlines) {
    auto page = outline.getDestPage();
    auto key = page.getObjGen();
    if (!pageIndices.contains(key)) {
      continue;
    }
    inserter = {
      .mName = outline.getTitle(),
      .mPageIndex = pageIndices.at(key),
    };

    auto kids = outline.getKids();
    ExtractBookmarks(kids, pageIndices, inserter);
  }
}

static std::vector<Bookmark> ExtractBookmarks(
  QPDFOutlineDocumentHelper& outlineHelper,
  const PageIndexMap& pageIndices) {
  std::vector<Bookmark> bookmarks;
  DebugTimer timer("Bookmarks");
  auto outlines = outlineHelper.getTopLevelOutlines();
  ExtractBookmarks(outlines, pageIndices, std::back_inserter(bookmarks));
  return bookmarks;
}

static bool PushDestLink(
  QPDFObjectHandle& it,
  const PageIndexMap& pageIndices,
  const NormalizedRect& rect,
  std::vector<Link>& links) {
  if (!it.hasKey("/Dest")) {
    return false;
  }

  auto dest = it.getKey("/Dest");
  auto destPage = pageIndices.at(dest.getArrayItem(0).getObjGen());
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
  const NormalizedRect& rect,
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
  const NormalizedRect& rect,
  std::vector<Link>& links) {
  if (!action.hasKey("/D")) {
    return;
  }

  auto dest = action.getKey("/D");
  if (!(dest.isName() || dest.isString())) {
    return;
  }
  dest = outlineHelper.resolveNamedDest(dest);
  if (!dest.isArray()) {
    return;
  }

  auto destPage = pageIndices.at(dest.getArrayItem(0).getObjGen());
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
  const NormalizedRect& rect,
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
    const NormalizedRect linkRect {
      .mLeft = static_cast<float>(pdfRect.llx - pageRect.llx) / pageWidth,
      .mTop
      = 1.0f - (static_cast<float>(pdfRect.ury - pageRect.lly) / pageHeight),
      .mRight = static_cast<float>(pdfRect.urx - pageRect.llx) / pageWidth,
      .mBottom
      = 1.0f - (static_cast<float>(pdfRect.lly - pageRect.lly) / pageHeight),
    };

    auto handle = annotation.getObjectHandle();
    if (PushDestLink(handle, pageIndices, linkRect, links)) {
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

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int) {
  DPrintSettings::Set({
    .prefix = "OpenKneeboard-PDF-Helper",
  });

  int argc = 0;
  auto argv = CommandLineToArgvW(commandLine, &argc);

  if (argc != 1) {
    dprintf("Expected exactly 1 argument, got {}", argc);
    return 1;
  }

  const std::filesystem::path requestPath(argv[0]);
  dprintf(L"Request file: {}", requestPath.wstring());

  if (!std::filesystem::is_regular_file(requestPath)) {
    dprint("Request file does not exist.");
    return 2;
  }

  const auto request
    = nlohmann::json::parse(std::ifstream(requestPath.wstring()))
        .get<Request>();

  const std::filesystem::path pdfPath(
    std::wstring_view {winrt::to_hstring(request.mPDFFilePath)});

  if (!std::filesystem::is_regular_file(pdfPath)) {
    dprintf(L"Can't find PDF file {}", pdfPath.wstring());
    return 3;
  }

  DebugTimer totalTimer(std::format("Total ({})", to_utf8(pdfPath.filename())));
  DebugTimer initTimer("Init");

  const auto fileSize = std::filesystem::file_size(pdfPath);
  const auto wpath = pdfPath.wstring();
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
    return 4;
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
    return 5;
  }
  auto fileView = MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, fileSize);
  scope_guard fileViewGuard([fileView]() { UnmapViewOfFile(fileView); });

  QPDF qpdf;
  const auto utf8Path = to_utf8(pdfPath);
  qpdf.processMemoryFile(
    utf8Path.c_str(), reinterpret_cast<const char*>(fileView), fileSize);
  auto pages = QPDFPageDocumentHelper(qpdf).getAllPages();
  PageIndexMap pageIndices;
  for (const auto& page: pages) {
    pageIndices[page.getObjectHandle().getObjGen()] = pageIndices.size();
  }
  QPDFOutlineDocumentHelper outlineHelper(qpdf);

  initTimer.End();

  const Response response {
    .mPDFFilePath = request.mPDFFilePath,
    .mBookmarks = ExtractBookmarks(outlineHelper, pageIndices),
    .mLinksByPage = ExtractLinks(outlineHelper, pages, pageIndices),
  };

  std::ofstream output(
    std::wstring {winrt::to_hstring(request.mOutputFilePath)});
  output << std::setw(2) << nlohmann::json(response) << std::endl;

  return 0;
}
