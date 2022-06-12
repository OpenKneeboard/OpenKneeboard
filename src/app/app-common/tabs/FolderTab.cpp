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
#include <OpenKneeboard/FolderTab.h>
#include <OpenKneeboard/NavigationTab.h>
#include <OpenKneeboard/dprint.h>
#include <wincodec.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <nlohmann/json.hpp>

using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Search;

namespace OpenKneeboard {

FolderTab::FolderTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view /* title */,
  const std::filesystem::path& path)
  : TabWithDoodles(dxr, kbs),
    mDXR {dxr},
    mWIC {winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory)},
    mPath {path} {
  this->Reload();
}

FolderTab::FolderTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view title,
  const nlohmann::json& settings)
  : FolderTab(
    dxr,
    kbs,
    title,
    settings.at("Path").get<std::filesystem::path>()) {
}

FolderTab::~FolderTab() {
}

nlohmann::json FolderTab::GetSettings() const {
  return {{"Path", GetPath()}};
}

utf8_string FolderTab::GetGlyph() const {
  return "\uE838";
}

utf8_string FolderTab::GetTitle() const {
  return mPath.filename();
}

bool FolderTab::CanOpenFile(const std::filesystem::path& path) const {
  if (!std::filesystem::is_regular_file(path)) {
    return false;
  }
  auto wsPath = path.wstring();
  winrt::com_ptr<IWICBitmapDecoder> decoder;
  mWIC->CreateDecoderFromFilename(
    wsPath.c_str(),
    nullptr,
    GENERIC_READ,
    WICDecodeMetadataCacheOnLoad,
    decoder.put());
  return static_cast<bool>(decoder);
}

void FolderTab::Reload() {
  this->ReloadImpl();
}

winrt::fire_and_forget FolderTab::ReloadImpl() noexcept {
  co_await mUIThread;

  if (mPath.empty() || !std::filesystem::is_directory(mPath)) {
    mPages.clear();
    evFullyReplacedEvent.EmitFromMainThread();
    co_return;
  }
  if ((!mQueryResult) || mPath != mQueryResult.Folder().Path()) {
    mQueryResult = nullptr;
    auto folder
      = co_await StorageFolder::GetFolderFromPathAsync(mPath.wstring());
    mQueryResult = folder.CreateFileQuery(CommonFileQuery::OrderByName);
    mQueryResult.ContentsChanged(
      [this](const auto&, const auto&) { this->ReloadImpl(); });
  }
  auto files = co_await mQueryResult.GetFilesAsync();

  co_await winrt::resume_background();

  std::vector<Page> pages;
  for (const auto& file: files) {
    std::filesystem::path path(std::wstring_view {file.Path()});
    if (!this->CanOpenFile(path)) {
      continue;
    }
    pages.push_back({.mPath = path});
  }

  co_await mUIThread;

  mPages = pages;
  evFullyReplacedEvent.Emit();
}

uint16_t FolderTab::GetPageCount() const {
  return static_cast<uint16_t>(mPages.size());
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

D2D1_SIZE_U FolderTab::GetNativeContentSize(uint16_t index) {
  if (!IsValidPageIndex(index, GetPageCount())) {
    return {};
  }

  auto bitmap = GetPageBitmap(index);
  if (!bitmap) {
    return {};
  }

  return bitmap->GetPixelSize();
}

void FolderTab::RenderPageContent(
  ID2D1DeviceContext* ctx,
  uint16_t index,
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

winrt::com_ptr<ID2D1Bitmap> FolderTab::GetPageBitmap(uint16_t index) {
  if (index >= mPages.size()) {
    return {};
  }

  auto& page = mPages.at(index);
  if (page.mBitmap) {
    return page.mBitmap;
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
  mDXR.mD2DDeviceContext->CreateBitmapFromWicBitmap(
    converter.get(), sharedBitmap.put());
  if (!sharedBitmap) {
    return {};
  }

  winrt::check_hresult(mDXR.mD2DDeviceContext->CreateBitmap(
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

std::filesystem::path FolderTab::GetPath() const {
  return mPath;
}

void FolderTab::SetPath(const std::filesystem::path& path) {
  if (path == mPath) {
    return;
  }
  mPath = path;
  mQueryResult = nullptr;
  this->Reload();
}

bool FolderTab::IsNavigationAvailable() const {
  return mPages.size();
}

std::shared_ptr<Tab> FolderTab::CreateNavigationTab(uint16_t currentPage) {
  std::vector<NavigationTab::Entry> entries;

  for (uint16_t i = 0; i < mPages.size(); ++i) {
    entries.push_back({mPages.at(i).mPath.stem(), i});
  }

  return std::make_shared<NavigationTab>(
    mDXR, this, entries, this->GetNativeContentSize(currentPage));
}

}// namespace OpenKneeboard
