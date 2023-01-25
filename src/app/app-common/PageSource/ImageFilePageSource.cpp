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
#include <OpenKneeboard/ImageFilePageSource.h>
#include <OpenKneeboard/dprint.h>
#include <wincodec.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <ranges>

using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Search;

namespace OpenKneeboard {

ImageFilePageSource::ImageFilePageSource(
  const DXResources& dxr,
  const std::vector<std::filesystem::path>& paths)
  : mDXR(dxr) {
  this->SetPaths(paths);
}

void ImageFilePageSource::SetPaths(
  const std::vector<std::filesystem::path>& paths) {
  mPages.clear();
  mPages.reserve(paths.size());
  for (const auto& path: paths) {
    mPages.push_back({.mPath = path});
  }
}

std::vector<std::filesystem::path> ImageFilePageSource::GetPaths() const {
  auto view = std::ranges::views::transform(
    mPages, [](const auto& page) { return page.mPath; });
  return {view.begin(), view.end()};
}

ImageFilePageSource::~ImageFilePageSource() = default;

bool ImageFilePageSource::CanOpenFile(const std::filesystem::path& path) const {
  return ImageFilePageSource::CanOpenFile(mDXR, path);
}

bool ImageFilePageSource::CanOpenFile(
  const DXResources& dxr,
  const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(path)) {
    return false;
  }
  auto wsPath = path.wstring();
  winrt::com_ptr<IWICBitmapDecoder> decoder;
  dxr.mWIC->CreateDecoderFromFilename(
    wsPath.c_str(),
    nullptr,
    GENERIC_READ,
    WICDecodeMetadataCacheOnLoad,
    decoder.put());
  return static_cast<bool>(decoder);
}

PageIndex ImageFilePageSource::GetPageCount() const {
  return static_cast<PageIndex>(mPages.size());
}

static bool IsValidPageIndex(PageIndex index, PageIndex count) {
  if (index < count) {
    return true;
  }

  if (index > 0) {
    dprintf("Asked for page {} >= pagecount {} in {}", index, count, __FILE__);
  }

  return false;
}

D2D1_SIZE_U ImageFilePageSource::GetNativeContentSize(PageIndex index) {
  if (!IsValidPageIndex(index, GetPageCount())) {
    return {};
  }

  auto bitmap = GetPageBitmap(index);
  if (!bitmap) {
    return {};
  }

  return bitmap->GetPixelSize();
}

void ImageFilePageSource::RenderPage(
  RenderTargetID,
  ID2D1DeviceContext* ctx,
  PageIndex index,
  const D2D1_RECT_F& rect) {
  if (!IsValidPageIndex(index, GetPageCount())) {
    return;
  }

  auto bitmap = GetPageBitmap(index);
  if (!bitmap) {
    return;
  }
  const auto pageSize = bitmap->GetPixelSize();

  const auto targetWidth = rect.right - rect.left;
  const auto targetHeight = rect.bottom - rect.top;
  const auto scaleX = float(targetWidth) / pageSize.width;
  const auto scaleY = float(targetHeight) / pageSize.height;
  const auto scale = std::min(scaleX, scaleY);

  const auto renderWidth = pageSize.width * scale;
  const auto renderHeight = pageSize.height * scale;

  const auto renderLeft = rect.left + ((targetWidth - renderWidth) / 2);
  const auto renderTop = rect.top + ((targetHeight - renderHeight) / 2);

  ctx->DrawBitmap(
    bitmap.get(),
    {renderLeft, renderTop, renderLeft + renderWidth, renderTop + renderHeight},
    1.0f,
    D2D1_INTERPOLATION_MODE_ANISOTROPIC);
}

winrt::com_ptr<ID2D1Bitmap> ImageFilePageSource::GetPageBitmap(
  PageIndex index) {
  std::unique_lock lock(mMutex);
  if (index >= mPages.size()) [[unlikely]] {
    return {};
  }

  auto& page = mPages.at(index);
  if (page.mBitmap) [[likely]] {
    return page.mBitmap;
  }

  winrt::com_ptr<IWICBitmapDecoder> decoder;

  const auto wic = mDXR.mWIC.get();

  auto path = page.mPath.wstring();
  wic->CreateDecoderFromFilename(
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
  wic->CreateFormatConverter(converter.put());
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

  /* `CreateBitmapFromWicBitmap` creates a Direct2D bitmap that refers to
   * - and retains a reference to - the existing WIC bitmap.
   *
   * This means that Direct2D/Direct3D indirectly keep a reference to an
   * object that keeps an open file alive; releasing the internal references
   * is not enough to close the file - we need to also wait for Direct2D/3D to
   * finish.
   *
   * This is problematic if this `FolderTab` is pointing at a temporary folder
   * if we later want to delete the temporary folder: even after calling
   * `SetPath({})`, DirectX may still hold references to the Direct2D bitmap,
   * and in turn the WIC bitmap and open file.
   *
   * The simplest way to deal with this problem is to do an immediate on-GPU
   * copy of the pixel data from the WIC-referencing bitmap to an independent
   * bitmap - then the WIC bitmap can immediately be freed, and is never
   * referenced (or kept alive) by the D3D11 render pipeline.
   */
  winrt::com_ptr<ID2D1Bitmap> sharedBitmap;
  winrt::com_ptr<ID2D1DeviceContext> ctx;
  winrt::check_hresult(mDXR.mD2DDevice->CreateDeviceContext(
    D2D1_DEVICE_CONTEXT_OPTIONS_NONE, ctx.put()));

  ctx->CreateBitmapFromWicBitmap(converter.get(), sharedBitmap.put());
  if (!sharedBitmap) {
    return {};
  }

  winrt::check_hresult(ctx->CreateBitmap(
    sharedBitmap->GetPixelSize(),
    D2D1_BITMAP_PROPERTIES {
      .pixelFormat = {
        .format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED,
      },
    },
    page.mBitmap.put()));

  if (!page.mBitmap) {
    return {};
  }
  page.mBitmap->CopyFromBitmap(nullptr, sharedBitmap.get(), nullptr);

  return page.mBitmap;
}

bool ImageFilePageSource::IsNavigationAvailable() const {
  return this->GetPageCount() > 2;
}

std::vector<NavigationEntry> ImageFilePageSource::GetNavigationEntries() const {
  std::vector<NavigationEntry> entries;
  for (PageIndex i = 0; i < mPages.size(); ++i) {
    const auto& page = mPages.at(i);
    entries.push_back({
      to_utf8(page.mPath.stem()),
      i,
    });
  }
  return entries;
}

}// namespace OpenKneeboard
