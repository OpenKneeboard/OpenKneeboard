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
    winrt::com_ptr<IWICBitmapSource> mWICBitmap;
  };

  DXResources mDXR;
  winrt::com_ptr<IWICImagingFactory> mWIC;
  std::filesystem::path mPath;
  std::vector<Page> mPages = {};

  winrt::com_ptr<IWICBitmapSource> GetPageBitmap(uint16_t index);
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

  auto bitmap = p->GetPageBitmap(index);
  if (!bitmap) {
    return {};
  }
  UINT width, height;
  winrt::check_hresult(bitmap->GetSize(&width, &height));

  return {width, height};
}

void FolderTab::RenderPageContent(uint16_t index, const D2D1_RECT_F& rect) {
  if (!IsValidPageIndex(index, GetPageCount())) {
    return;
  }

  auto wicBitmap = p->GetPageBitmap(index);
  if (!wicBitmap) {
    return;
  }
  UINT pageWidth, pageHeight;
  winrt::check_hresult(wicBitmap->GetSize(&pageWidth, &pageHeight));

  winrt::com_ptr<ID2D1Bitmap> d2dBitmap;
  auto ctx = p->mDXR.mD2DDeviceContext;
  ctx->CreateBitmapFromWicBitmap(wicBitmap.get(), d2dBitmap.put());


  const auto targetWidth = rect.right - rect.left;
  const auto targetHeight = rect.bottom - rect.top;
  const auto scaleX = float(targetWidth) / pageWidth;
  const auto scaleY = float(targetHeight) / pageHeight;
  const auto scale = std::min(scaleX, scaleY);

  const auto renderWidth = pageWidth * scale;
  const auto renderHeight = pageHeight * scale;

  const auto renderLeft = rect.left + ((targetWidth - renderWidth) / 2);
  const auto renderTop = rect.top + ((targetHeight - renderHeight) / 2);

  ctx->DrawBitmap(
    d2dBitmap.get(),
    {renderLeft, renderTop, renderLeft + renderWidth, renderTop + renderHeight},
    1.0f,
    D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
}

winrt::com_ptr<IWICBitmapSource> FolderTab::Impl::GetPageBitmap(uint16_t index) {
  if (index >= mPages.size()) {
    return {};
  }

  auto& page = mPages.at(index);
  if (page.mWICBitmap) {
    return page.mWICBitmap;
  }


  winrt::com_ptr<IWICBitmapDecoder> decoder;

  auto path = page.mPath.wstring();
  mWIC->CreateDecoderFromFilename(
    path.c_str(),
    nullptr,
    GENERIC_READ,
    WICDecodeMetadataCacheOnLoad,
    decoder.put());
  if (!decoder) {
    return {};
  }

  winrt::com_ptr<IWICBitmapFrameDecode> frame;
  decoder->GetFrame(0, frame.put());
  if (!frame) {
    return {};
  }

  winrt::com_ptr<IWICFormatConverter> converter;
  mWIC->CreateFormatConverter(converter.put());
  if (!converter) {
    return {};
  }
  converter->Initialize(
    frame.get(),
    GUID_WICPixelFormat32bppPBGRA,
    WICBitmapDitherTypeNone,
    nullptr,
    0.0f,
    WICBitmapPaletteTypeMedianCut);

  page.mWICBitmap = converter;
  return page.mWICBitmap;
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
