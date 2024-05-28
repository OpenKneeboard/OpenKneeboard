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

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include <ranges>

#include <wincodec.h>

namespace OpenKneeboard {

std::shared_ptr<ImageFilePageSource> ImageFilePageSource::Create(
  const audited_ptr<DXResources>& dxr,
  const std::vector<std::filesystem::path>& paths) {
  auto ret = std::shared_ptr<ImageFilePageSource>(new ImageFilePageSource(dxr));
  ret->SetPaths(paths);
  return ret;
}

ImageFilePageSource::ImageFilePageSource(const audited_ptr<DXResources>& dxr)
  : mDXR(dxr) {
}

void ImageFilePageSource::SetPaths(
  const std::vector<std::filesystem::path>& paths) {
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "ImageFilePageSource::SetPaths()");
  mPages.clear();
  mPages.reserve(paths.size());
  for (const auto& path: paths) {
    auto watcher = FilesystemWatcher::Create(path);

    AddEventListener(
      watcher->evFilesystemModifiedEvent,
      [weak = weak_from_this()](const auto& path) {
        if (auto self = weak.lock()) {
          self->OnFileModified(path);
        }
      });

    mPages.push_back({
      .mPath = path,
      .mWatcher = watcher,
    });
    TraceLoggingWriteTagged(
      activity,
      "ImageFilePageSource::SetPaths()/Page",
      TraceLoggingValue(path.c_str(), "Path"),
      TraceLoggingHexUInt64(mPages.back().mID.GetTemporaryValue(), "PageID"));
  }
}

void ImageFilePageSource::OnFileModified(const std::filesystem::path& path) {
  auto it = std::ranges::find_if(
    mPages, [&path](auto& page) { return page.mPath == path; });
  if (it == mPages.end()) {
    return;
  }
  if (std::filesystem::exists(path)) {
    it->mBitmap = {};
    it->mID = {};
  } else {
    mPages.erase(it);
  }
  this->evContentChangedEvent.Emit();
}

std::vector<std::filesystem::path> ImageFilePageSource::GetPaths() const {
  auto view = std::ranges::views::transform(
    mPages, [](const auto& page) { return page.mPath; });
  return {view.begin(), view.end()};
}

ImageFilePageSource::~ImageFilePageSource() {
  this->RemoveAllEventListeners();
}

bool ImageFilePageSource::CanOpenFile(const std::filesystem::path& path) const {
  return ImageFilePageSource::CanOpenFile(mDXR, path);
}

bool ImageFilePageSource::CanOpenFile(
  const audited_ptr<DXResources>& dxr,
  const std::filesystem::path& path) {
  try {
    if (!std::filesystem::is_regular_file(path)) {
      return false;
    }
  } catch (const std::filesystem::filesystem_error& e) {
    dprintf(
      "ImageFilePageSource failed to get status of file '{}': {} ({})",
      path,
      e.what(),
      e.code().value());
    return false;
  }
  auto wsPath = path.wstring();
  winrt::com_ptr<IWICBitmapDecoder> decoder;
  dxr->mWIC->CreateDecoderFromFilename(
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

std::vector<PageID> ImageFilePageSource::GetPageIDs() const {
  std::vector<PageID> ret;
  for (const auto& page: mPages) {
    ret.push_back(page.mID);
  }
  return ret;
}

PreferredSize ImageFilePageSource::GetPreferredSize(PageID pageID) {
  auto bitmap = GetPageBitmap(pageID);
  if (!bitmap) {
    return {};
  }

  const auto size = bitmap->GetPixelSize();

  return {{size.width, size.height}, ScalingKind::Bitmap};
}

void ImageFilePageSource::RenderPage(
  RenderTarget* rt,
  PageID pageID,
  const PixelRect& rect) {
  auto bitmap = GetPageBitmap(pageID);
  if (!bitmap) {
    return;
  }
  const auto pageSize = bitmap->GetPixelSize();

  const auto renderSize
    = PixelSize(pageSize.width, pageSize.height).ScaledToFit(rect.mSize);

  const auto renderLeft
    = rect.Left() + ((rect.Width() - renderSize.Width()) / 2);
  const auto renderTop
    = rect.Top() + ((rect.Height() - renderSize.Height()) / 2);

  auto ctx = rt->d2d();
  ctx->DrawBitmap(
    bitmap.get(),
    PixelRect {{renderLeft, renderTop}, renderSize},
    1.0f,
    D2D1_INTERPOLATION_MODE_ANISOTROPIC);
}

winrt::com_ptr<ID2D1Bitmap> ImageFilePageSource::GetPageBitmap(PageID pageID) {
  std::unique_lock lock(mMutex);
  auto it = std::ranges::find_if(
    mPages, [pageID](const auto& page) { return page.mID == pageID; });
  if (it == mPages.end()) [[unlikely]] {
    return {};
  }

  auto& page = *it;
  if (page.mBitmap) [[likely]] {
    return page.mBitmap;
  }

  winrt::com_ptr<IWICBitmapDecoder> decoder;

  const auto wic = mDXR->mWIC.get();

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
  winrt::check_hresult(mDXR->mD2DDevice->CreateDeviceContext(
    D2D1_DEVICE_CONTEXT_OPTIONS_NONE, ctx.put()));

  ctx->CreateBitmapFromWicBitmap(converter.get(), sharedBitmap.put());
  if (!sharedBitmap) {
    return {};
  }

  // For WIC, this MUST be B8G8R8A8_UNORM, not _UNORM_SRGB, or the copy silently
  // fails.
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
      page.mID,
    });
  }
  return entries;
}

}// namespace OpenKneeboard
