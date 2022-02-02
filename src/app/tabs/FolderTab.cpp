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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <OpenKneeboard/FolderTab.h>
#include <OpenKneeboard/dprint.h>
#include <wincodec.h>

#include "okEvents.h"

namespace OpenKneeboard {
class FolderTab::Impl final {
 public:
  struct Page {
    std::filesystem::path path;
    unsigned int width = 0;
    unsigned int height = 0;
    std::vector<std::byte> pixels;

    operator bool() const {
      return !pixels.empty();
    }
  };
  winrt::com_ptr<IWICImagingFactory> wic;
  std::filesystem::path path;
  std::vector<Page> pages = {};

  bool LoadPage(uint16_t index);
};

FolderTab::FolderTab(const wxString& title, const std::filesystem::path& path)
  : Tab(title),
    p(new Impl {
      .wic
      = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory),
      .path = path}) {
  Reload();
}

FolderTab::~FolderTab() {
}

void FolderTab::Reload() {
  p->pages.clear();
  if (!std::filesystem::is_directory(p->path)) {
    return;
  }
  for (auto& entry: std::filesystem::recursive_directory_iterator(p->path)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    auto wsPath = entry.path().wstring();
    if (!wxImage::CanRead(wsPath)) {
      continue;
    }
    p->pages.push_back({.path = wsPath});
  }
  wxQueueEvent(this, new wxCommandEvent(okEVT_TAB_FULLY_REPLACED));
}

uint16_t FolderTab::GetPageCount() const {
  return p->pages.size();
}

static bool IsValidPageIndex(uint16_t index, uint16_t count) {
  if (index < count) {
    return true;
  }

  if (index > 0) {
    dprintf("Asked for page {} >= pagecount {} in {}", index, count, __FILE__);
  }

  return false;
}

D2D1_SIZE_U FolderTab::GetPreferredPixelSize(uint16_t index) {
  if (!IsValidPageIndex(index, GetPageCount())) {
    return {};
  }

  auto& page = p->pages.at(index);
  if (!page) {
    if (!p->LoadPage(index)) {
      return {};
    }
  }

  return {page.width, page.height};
}

void FolderTab::RenderPage(
  uint16_t index,
  const winrt::com_ptr<ID2D1RenderTarget>& rt,
  const D2D1_RECT_F& rect) {
  if (!IsValidPageIndex(index, GetPageCount())) {
    return;
  }

  auto& page = p->pages.at(index);
  if (!page) {
    if (!p->LoadPage(index)) {
      return;
    }
  }

  winrt::com_ptr<ID2D1Bitmap> bmp;
  rt->CreateBitmap(
    {page.width, page.height},
    reinterpret_cast<const void*>(page.pixels.data()),
    page.width * 4,
    D2D1_BITMAP_PROPERTIES {
      {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 0, 0},
    bmp.put());

  const auto targetWidth = rect.right - rect.left;
  const auto targetHeight = rect.bottom - rect.top;
  const auto scaleX = float(targetWidth) / page.width;
  const auto scaleY = float(targetHeight) / page.height;
  const auto scale = std::min(scaleX, scaleY);

  const auto renderWidth = page.width * scale;
  const auto renderHeight = page.height * scale;

  const auto renderLeft = rect.left + ((targetWidth - renderWidth) / 2);
  const auto renderTop = rect.top + ((targetHeight - renderHeight) / 2);

  rt->DrawBitmap(
    bmp.get(),
    {renderLeft, renderTop, renderLeft + renderWidth, renderTop + renderHeight},
    1.0f,
    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

bool FolderTab::Impl::LoadPage(uint16_t index) {
  winrt::com_ptr<IWICBitmapDecoder> decoder;
  auto p = this;

  auto path = p->pages.at(index).path.wstring();
  p->wic->CreateDecoderFromFilename(
    path.c_str(),
    nullptr,
    GENERIC_READ,
    WICDecodeMetadataCacheOnLoad,
    decoder.put());
  if (!decoder) {
    return false;
  }

  winrt::com_ptr<IWICBitmapFrameDecode> frame;
  decoder->GetFrame(0, frame.put());
  if (!frame) {
    return false;
  }

  winrt::com_ptr<IWICFormatConverter> converter;
  p->wic->CreateFormatConverter(converter.put());
  if (!converter) {
    return false;
  }
  converter->Initialize(
    frame.get(),
    GUID_WICPixelFormat32bppBGRA,
    WICBitmapDitherTypeNone,
    nullptr,
    0.0f,
    WICBitmapPaletteTypeMedianCut);

  UINT width, height;
  frame->GetSize(&width, &height);
  auto& page = p->pages.at(index);
  page.width = width;
  page.height = height;
  page.pixels.resize(width * height * 4 /* BGRA */);
  converter->CopyPixels(
    nullptr,
    width * 4,
    page.pixels.size(),
    reinterpret_cast<BYTE*>(page.pixels.data()));

  return true;
}

std::filesystem::path FolderTab::GetPath() const {
  return p->path;
}

void FolderTab::SetPath(const std::filesystem::path& path) {
  if (path == p->path) {
    return;
  }
  p->path = path;
  Reload();
}

}// namespace OpenKneeboard
