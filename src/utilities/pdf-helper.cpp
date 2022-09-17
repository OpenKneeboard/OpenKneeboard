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
#include <fpdf_doc.h>
#include <fpdf_scopers.h>
#include <fpdfview.h>
#include <shellapi.h>
#include <shims/winrt/base.h>

#include <fstream>
#include <shims/filesystem>

using namespace OpenKneeboard;
using namespace OpenKneeboard::PDFIPC;

static void ExtractBookmarks(
  FPDF_DOCUMENT document,
  FPDF_BOOKMARK parent,
  std::back_insert_iterator<std::vector<Bookmark>> inserter) {
  for (auto it = FPDFBookmark_GetFirstChild(document, parent); it;
       it = FPDFBookmark_GetNextSibling(document, it)) {
    auto dest = FPDFBookmark_GetDest(document, it);
    if (!dest) {
      continue;
    }
    const auto page = FPDFDest_GetDestPageIndex(document, dest);
    if (page == -1) {
      continue;
    }

    const auto titleBytes
      = static_cast<size_t>(FPDFBookmark_GetTitle(it, nullptr, 0));
    std::wstring title(titleBytes / sizeof(wchar_t), L'\0');
    FPDFBookmark_GetTitle(it, title.data(), titleBytes);

    std::wstring_view titleView {title};
    titleView.remove_suffix(1);// trailing null

    inserter = {
      to_utf8(titleView),
      static_cast<uint16_t>(page),
    };

    ExtractBookmarks(document, it, inserter);
  }
}

static std::vector<Bookmark> ExtractBookmarks(FPDF_DOCUMENT document) {
  std::vector<Bookmark> bookmarks;
  DebugTimer timer("Bookmarks");
  ExtractBookmarks(
    document, /* parent = */ nullptr, std::back_inserter(bookmarks));
  return bookmarks;
}

static void PushDestLink(
  FPDF_DOCUMENT document,
  FPDF_DEST dest,
  const NormalizedRect& rect,
  std::vector<Link>& container) {
  if (!dest) {
    return;
  }
  const auto destPage = FPDFDest_GetDestPageIndex(document, dest);
  if (destPage < 0) {
    return;
  }

  container.push_back({
    rect,
    {
      .mType = DestinationType::Page,
      .mPageIndex = static_cast<uint16_t>(destPage),
    },
  });
}

static void PushURIActionLink(
  FPDF_DOCUMENT document,
  FPDF_ACTION action,
  const NormalizedRect& rect,
  std::vector<Link>& container) {
  const auto byteLen = FPDFAction_GetURIPath(document, action, nullptr, 0);
  if (byteLen < 2) {
    // at least 1 meaningful byte + trailing null
    return;
  }

  std::string uri(byteLen, '\0');
  FPDFAction_GetURIPath(document, action, uri.data(), byteLen);
  uri.resize(uri.size() - 1);// remove trailing null

  container.push_back({
    rect,
    {
      .mType = DestinationType::URI,
      .mURI = uri,
    },
  });
}

static std::vector<std::vector<Link>> ExtractLinks(FPDF_DOCUMENT document) {
  DebugTimer timer("Links");
  std::vector<std::vector<Link>> allLinks;
  const auto pageCount = FPDF_GetPageCount(document);
  allLinks.reserve(pageCount);

  for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
    std::vector<Link> pageLinks;
    const scope_guard pushPageLinks([&]() { allLinks.push_back(pageLinks); });

    ScopedFPDFPage page {FPDF_LoadPage(document, pageIndex)};

    int startPos = 0;
    FPDF_LINK link {nullptr};
    FPDFLink_Enumerate(page.get(), &startPos, &link);
    if (!link) {
      // continue;
    }

    FS_RECTF pageRect;
    if (!FPDF_GetPageBoundingBox(page.get(), &pageRect)) {
      continue;
    }

    // PDF origin is bottom left, not top left
    const auto pageWidth = pageRect.right - pageRect.left;
    const auto pageHeight = pageRect.top - pageRect.bottom;

    while (link) {
      const scope_guard next([&]() {
        if (!FPDFLink_Enumerate(page.get(), &startPos, &link)) {
          link = nullptr;
        }
      });

      FS_RECTF linkRect;
      if (!FPDFLink_GetAnnotRect(link, &linkRect)) {
        continue;
      }

      // Rect dicts are usually [llx, lly, urx, ury], but PDFium
      // normalizes them so that bottom > top
      //
      // Get us back to the PDF coordinate system (even though it's weird),
      // so things are consistent
      if (linkRect.bottom > linkRect.top) {
        std::swap(linkRect.bottom, linkRect.top);
      }
      if (linkRect.left > linkRect.right) {
        std::swap(linkRect.left, linkRect.right);
      }

      const NormalizedRect normalizedLinkRect {
        .mLeft = (linkRect.left - pageRect.left) / pageWidth,
        .mTop = 1 - ((linkRect.top - pageRect.bottom) / pageHeight),
        .mRight = (linkRect.right - pageRect.left) / pageWidth,
        .mBottom = 1 - ((linkRect.bottom - pageRect.bottom) / pageHeight),
      };

      auto dest = FPDFLink_GetDest(document, link);
      if (dest) {
        PushDestLink(document, dest, normalizedLinkRect, pageLinks);
        continue;
      }

      const auto action = FPDFLink_GetAction(link);
      if (action) {
        switch (FPDFAction_GetType(action)) {
          case PDFACTION_GOTO:
            PushDestLink(
              document,
              FPDFAction_GetDest(document, action),
              normalizedLinkRect,
              pageLinks);
            continue;
          case PDFACTION_URI:
            PushURIActionLink(document, action, normalizedLinkRect, pageLinks);
            continue;
          default:
            continue;
        }
      }
    }
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

  const auto pdfFile = pdfPath.filename().string();
  DebugTimer totalTimer(std::format("Total ({})", pdfFile));
  DebugTimer initTimer("Init");

  FPDF_InitLibrary();

  const ScopedFPDFDocument document {
    FPDF_LoadDocument(to_utf8(pdfPath).c_str(), nullptr)};
  initTimer.End();

  const Response response {
    .mPDFFilePath = request.mPDFFilePath,
    .mBookmarks = ExtractBookmarks(document.get()),
    .mLinksByPage = ExtractLinks(document.get()),
  };

  std::ofstream output(
    std::wstring {winrt::to_hstring(request.mOutputFilePath)});
  output << std::setw(2) << nlohmann::json(response) << std::endl;

  return 0;
}
