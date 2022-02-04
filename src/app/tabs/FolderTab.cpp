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
#include <OpenKneeboard/FolderTab.h>
#include <OpenKneeboard/dprint.h>
#include <wincodec.h>

#include <OpenKneeboard/DXResources.h>
#include "okEvents.h"

namespace OpenKneeboard {
class FolderTab::Impl final {
 public:
  struct Page {
    std::filesystem::path mPath;
    unsigned int mWidth = 0;
    unsigned int mHeight = 0;
    std::vector<std::byte> mPixels;

    operator bool() const {
      return !mPixels.empty();
    }
  };
  DXResources mDXR;
  winrt::com_ptr<IWICImagingFactory> mWIC;
  std::filesystem::path mPath;
  std::vector<Page> mPages = {};

  bool LoadPage(uint16_t index);
};

FolderTab::FolderTab(
  const DXResources& dxr,
  const wxString& title,
  const std::filesystem::path& path)
  : Tab(dxr, title),
    p(new Impl {
      .mDXR = dxr,
      .mWIC
      = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory),
      .mPath = path}) {
  Reload();
}

FolderTab::~FolderTab() {
}

void FolderTab::Reload() {
  p->mPages.clear();
  if (!std::filesystem::is_directory(p->mPath)) {
    return;
  }
  for (auto& entry: std::filesystem::recursive_directory_iterator(p->mPath)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    auto wsPath = entry.path().wstring();
    if (!wxImage::CanRead(wsPath)) {
      continue;
    }
    p->mPages.push_back({.mPath = wsPath});
  }
  wxQueueEvent(this, new wxCommandEvent(okEVT_TAB_FULLY_REPLACED));
}

uint16_t FolderTab::GetPageCount() const {
  return p->mPages.size();
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

  auto& page = p->mPages.at(index);
  if (!page) {
    if (!p->LoadPage(index)) {
      return {};
    }
  }

  return {page.mWidth, page.mHeight};
}

void FolderTab::RenderPageContent(uint16_t index, const D2D1_RECT_F& rect) {
  if (!IsValidPageIndex(index, GetPageCount())) {
    return;
  }

  auto& page = p->mPages.at(index);
  if (!page) {
    if (!p->LoadPage(index)) {
      return;
    }
  }

  winrt::com_ptr<ID2D1Bitmap> bmp;
  auto ctx = p->mDXR.mD2DDeviceContext;
  ctx->CreateBitmap(
    {page.mWidth, page.mHeight},
    reinterpret_cast<const void*>(page.mPixels.data()),
    page.mWidth * 4,
    D2D1_BITMAP_PROPERTIES {
      {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 0, 0},
    bmp.put());

  const auto targetWidth = rect.right - rect.left;
  const auto targetHeight = rect.bottom - rect.top;
  const auto scaleX = float(targetWidth) / page.mWidth;
  const auto scaleY = float(targetHeight) / page.mHeight;
  const auto scale = std::min(scaleX, scaleY);

  const auto renderWidth = page.mWidth * scale;
  const auto renderHeight = page.mHeight * scale;

  const auto renderLeft = rect.left + ((targetWidth - renderWidth) / 2);
  const auto renderTop = rect.top + ((targetHeight - renderHeight) / 2);

  ctx->DrawBitmap(
    bmp.get(),
    {renderLeft, renderTop, renderLeft + renderWidth, renderTop + renderHeight},
    1.0f,
    D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
}

bool FolderTab::Impl::LoadPage(uint16_t index) {
  winrt::com_ptr<IWICBitmapDecoder> decoder;
  auto p = this;

  auto path = p->mPages.at(index).mPath.wstring();
  p->mWIC->CreateDecoderFromFilename(
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
  p->mWIC->CreateFormatConverter(converter.put());
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
  auto& page = p->mPages.at(index);
  page.mWidth = width;
  page.mHeight = height;
  page.mPixels.resize(width * height * 4 /* BGRA */);
  converter->CopyPixels(
    nullptr,
    width * 4,
    page.mPixels.size(),
    reinterpret_cast<BYTE*>(page.mPixels.data()));

  return true;
}

std::filesystem::path FolderTab::GetPath() const {
  return p->mPath;
}

void FolderTab::SetPath(const std::filesystem::path& path) {
  if (path == p->mPath) {
    return;
  }
  p->mPath = path;
  Reload();
}

}// namespace OpenKneeboard
