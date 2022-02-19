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
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/TabWithDoodles.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

#include "scope_guard.h"

namespace OpenKneeboard {

TabWithDoodles::TabWithDoodles(const DXResources& dxr) : mContentLayer(dxr) {
  mDXR = dxr;

  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), mBrush.put()));
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(1.0f, 0.0f, 1.0f, 0.0f), mEraser.put()));

  mDXR.mD2DDevice->CreateDeviceContext(
    D2D1_DEVICE_CONTEXT_OPTIONS_NONE, mDrawingContext.put());

  AddEventListener(
    this->evFullyReplacedEvent, &TabWithDoodles::ClearContentCache, this);
}

TabWithDoodles::~TabWithDoodles() {
}

void TabWithDoodles::ClearContentCache() {
  mContentLayer.Reset();
}

void TabWithDoodles::PostCursorEvent(
  const CursorEvent& event,
  uint16_t pageIndex) {
  if (pageIndex >= mDrawings.size()) {
    mDrawings.resize(std::max<uint16_t>(pageIndex + 1, GetPageCount()));
  }
  mDrawings.at(pageIndex).mBufferedEvents.push_back(event);
  this->evNeedsRepaintEvent.Emit();
}

void TabWithDoodles::FlushCursorEvents() {
  auto ctx = mDrawingContext;

  for (auto pageIndex = 0; pageIndex < mDrawings.size(); ++pageIndex) {
    auto& page = mDrawings.at(pageIndex);
    if (page.mBufferedEvents.empty()) {
      continue;
    }
    bool drawing = false;
    for (const auto& event: page.mBufferedEvents) {
      if (
        event.mTouchState != CursorTouchState::TOUCHING_SURFACE
        || event.mPositionState != CursorPositionState::IN_CONTENT_RECT) {
        page.mHaveCursor = false;
        continue;
      }

      if (!drawing) {
        drawing = true;
        ctx->BeginDraw();
        ctx->SetTarget(
          GetDrawingSurface(pageIndex, this->GetNativeContentSize(pageIndex)));
      }

      // ignore tip button - any other pen button == erase
      const bool erasing = event.mButtons & ~1;

      const auto scale = mDrawings.at(pageIndex).mScale;
      const auto pressure = std::clamp(event.mPressure - 0.40, 0.0, 0.60);
      auto radius = 1 + (pressure * 15);
      if (erasing) {
        radius *= 10;
      }
      const auto x = event.mX * scale;
      const auto y = event.mY * scale;
      const auto brush = erasing ? mEraser : mBrush;

      ctx->SetPrimitiveBlend(
        erasing ? D2D1_PRIMITIVE_BLEND_COPY : D2D1_PRIMITIVE_BLEND_SOURCE_OVER);

      if (page.mHaveCursor) {
        ctx->DrawLine(page.mCursorPoint, {x, y}, brush.get(), radius * 2);
      }
      ctx->FillEllipse(D2D1::Ellipse({x, y}, radius, radius), brush.get());
      page.mHaveCursor = true;
      page.mCursorPoint = {x, y};
    }
    if (drawing) {
      winrt::check_hresult(ctx->EndDraw());
    }
    page.mBufferedEvents.clear();
  }
}

ID2D1Bitmap* TabWithDoodles::GetDrawingSurface(
  uint16_t index,
  const D2D1_SIZE_U& contentPixels) {
  if (index >= mDrawings.size()) {
    mDrawings.resize(std::max<uint16_t>(index + 1, GetPageCount()));
  }

  auto& page = mDrawings.at(index);
  if (page.mBitmap) {
    return page.mBitmap.get();
  }

  const auto scaleX = static_cast<float>(TextureWidth) / contentPixels.width;
  const auto scaleY = static_cast<float>(TextureHeight) / contentPixels.height;
  page.mScale = std::min(scaleX, scaleY);
  D2D1_SIZE_U surfaceSize {
    static_cast<UINT32>(std::lround(contentPixels.width * page.mScale)),
    static_cast<UINT32>(std::lround(contentPixels.height * page.mScale)),
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

  winrt::com_ptr<ID3D11Texture2D> texture;
  winrt::check_hresult(
    mDXR.mD3DDevice->CreateTexture2D(&textureDesc, nullptr, texture.put()));
  page.mSurface = texture.as<IDXGISurface>();

  mDXR.mD2DDeviceContext->CreateBitmapFromDxgiSurface(
    page.mSurface.get(), nullptr, page.mBitmap.put());

  return page.mBitmap.get();
}

void TabWithDoodles::RenderPage(
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  FlushCursorEvents();

  const auto nativeSize = this->GetNativeContentSize(pageIndex);
  mContentLayer.Render(
    rect,
    nativeSize,
    pageIndex,
    ctx,
    [&](ID2D1DeviceContext* ctx, const D2D1_SIZE_U& size) {
      this->RenderPageContent(
        ctx,
        pageIndex,
        {0.0f,
         0.f,
         static_cast<FLOAT>(size.width),
         static_cast<FLOAT>(size.height)});
    });

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());

  if (pageIndex < mDrawings.size()) {
    auto& page = mDrawings.at(pageIndex);
    auto bitmap = page.mBitmap;

    if (bitmap) {
      ctx->DrawBitmap(
        bitmap.get(), rect, 1.0f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
    }
  }

  this->RenderOverDoodles(ctx, pageIndex, rect);
}

void TabWithDoodles::RenderOverDoodles(
  ID2D1DeviceContext*,
  uint16_t pageIndex,
  const D2D1_RECT_F&) {
}

}// namespace OpenKneeboard
