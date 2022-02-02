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
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/DrawableTab.h>
#include <OpenKneeboard/config.h>
#include <d2d1_1.h>
#include <d3d11_2.h>

namespace OpenKneeboard {

struct DrawableTab::Impl {
  winrt::com_ptr<IDXGIDevice2> mDevice;
  winrt::com_ptr<ID2D1Factory> mD2D;
  struct Page {
    winrt::com_ptr<ID3D11Texture2D> mTexture;
    winrt::com_ptr<ID2D1RenderTarget> mRenderTarget;
    winrt::com_ptr<ID2D1Bitmap> mBitmap;
    float mScale;
  };
  std::vector<Page> mPages;
  winrt::com_ptr<ID2D1SolidColorBrush> mCursorBrush;

  winrt::com_ptr<ID2D1RenderTarget> GetSurfaceRenderTarget(
    uint16_t index,
    const D2D1_SIZE_U& contentPixels);
};

DrawableTab::DrawableTab(const winrt::com_ptr<IDXGIDevice2>& dxgi)
  : p(std::make_shared<Impl>()) {
  p->mDevice = dxgi;
  D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, p->mD2D.put());
}

DrawableTab::~DrawableTab() {
}

void DrawableTab::OnCursorEvent(const CursorEvent& event, uint16_t pageIndex) {
  if (
    event.TouchState != CursorTouchState::TOUCHING_SURFACE
    || event.PositionState != CursorPositionState::IN_CLIENT_RECT) {
    return;
  }

  auto rt = p->GetSurfaceRenderTarget(
    pageIndex, this->GetPreferredPixelSize(pageIndex));

  if (!rt) {
    return;
  }

  if (!p->mCursorBrush) {
    rt->CreateSolidColorBrush(
      D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), p->mCursorBrush.put());
  }

  const auto scale = p->mPages.at(pageIndex).mScale;
  rt->BeginDraw();
  rt->FillEllipse(
    D2D1::Ellipse({event.x * scale, event.y * scale}, 5, 5),
    p->mCursorBrush.get());
  rt->EndDraw();
}

void DrawableTab::ClearDrawings() {
  p->mPages.clear();
}

void DrawableTab::RenderPage(
  uint16_t pageIndex,
  const winrt::com_ptr<ID2D1RenderTarget>& renderTarget,
  const D2D1_RECT_F& rect) {
  this->RenderPageContent(pageIndex, renderTarget, rect);

  if (p->mPages.size() <= pageIndex) {
    return;
  }

  auto& page = p->mPages.at(pageIndex);
  auto& bitmap = page.mBitmap;
  if (!bitmap) {
    if (!page.mTexture) {
      return;
    }

    D2D1_BITMAP_PROPERTIES bitmapProps {
      .pixelFormat
      = {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED},
    };
    auto err = renderTarget->CreateSharedBitmap(
      _uuidof(IDXGISurface),
      page.mTexture.as<IDXGISurface>().get(),
      &bitmapProps,
      bitmap.put());
    if (err) {
      OPENKNEEBOARD_BREAK;
    }
  }

  renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
  renderTarget->DrawBitmap(bitmap.get(), rect);
}

winrt::com_ptr<ID2D1RenderTarget> DrawableTab::Impl::GetSurfaceRenderTarget(
  uint16_t index,
  const D2D1_SIZE_U& contentPixels) {
  if (index >= mPages.size()) {
    mPages.resize(index + 1);
  }

  auto& page = mPages.at(index);
  auto& renderTarget = page.mRenderTarget;
  if (renderTarget) {
    return renderTarget;
  }

  const auto scaleX = static_cast<float>(TextureWidth) / contentPixels.width;
  const auto scaleY = static_cast<float>(TextureHeight) / contentPixels.height;
  page.mScale = std::min(scaleX, scaleY);
  D2D1_SIZE_U surfaceSize {
    std::lround(contentPixels.width * page.mScale),
    std::lround(contentPixels.height * page.mScale),
  };

  D3D11_TEXTURE2D_DESC textureDesc {
    .Width = surfaceSize.width,
    .Height = surfaceSize.height,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .SampleDesc = {1, 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
  };

  auto& texture = page.mTexture;
  mDevice.as<ID3D11Device>()->CreateTexture2D(
    &textureDesc, nullptr, texture.put());

  mD2D->CreateDxgiSurfaceRenderTarget(
    texture.as<IDXGISurface>().get(),
    D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_HARDWARE,
      D2D1::PixelFormat(
        DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
    renderTarget.put());

  return renderTarget;
}

}// namespace OpenKneeboard
