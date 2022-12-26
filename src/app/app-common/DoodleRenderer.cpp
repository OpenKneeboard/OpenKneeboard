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
#include <OpenKneeboard/DoodleRenderer.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

namespace OpenKneeboard {

DoodleRenderer::DoodleRenderer(const DXResources& dxr, KneeboardState* kbs)
  : mDXR(dxr), mKneeboard(kbs) {
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), mBrush.put()));
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(1.0f, 0.0f, 1.0f, 0.0f), mEraser.put()));

  mDXR.mD2DDevice->CreateDeviceContext(
    D2D1_DEVICE_CONTEXT_OPTIONS_NONE, mDrawingContext.put());
}

DoodleRenderer::~DoodleRenderer() = default;

void DoodleRenderer::Clear() {
  mDrawings.clear();
}

void DoodleRenderer::ClearPage(PageIndex pageIndex) {
  if (pageIndex >= mDrawings.size()) {
    return;
  }

  mDrawings.at(pageIndex) = {};
}

void DoodleRenderer::PostCursorEvent(
  EventContext,
  const CursorEvent& event,
  PageIndex pageIndex,
  const D2D1_SIZE_U& nativePageSize) {
  if (nativePageSize.width == 0 || nativePageSize.height == 0) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  {
    std::scoped_lock lock(mBufferedEventsMutex);
    if (pageIndex >= mDrawings.size()) {
      mDrawings.resize(pageIndex + 1);
    }
    auto& drawing = mDrawings.at(pageIndex);
    drawing.mNativeSize = nativePageSize;
    drawing.mBufferedEvents.push_back(event);
  }
  if (event.mButtons) {
    this->evNeedsRepaintEvent.Emit();
  }
}

void DoodleRenderer::FlushCursorEvents() {
  auto ctx = mDrawingContext;
  std::scoped_lock lock(mBufferedEventsMutex);

  for (auto pageIndex = 0; pageIndex < mDrawings.size(); ++pageIndex) {
    auto& page = mDrawings.at(pageIndex);
    if (page.mBufferedEvents.empty()) {
      continue;
    }

    bool drawing = false;
    for (const auto& event: page.mBufferedEvents) {
      if (event.mTouchState != CursorTouchState::TOUCHING_SURFACE) {
        page.mHaveCursor = false;
        continue;
      }

      if (!drawing) {
        drawing = true;
        ctx->BeginDraw();
        ctx->SetTarget(GetDrawingSurface(pageIndex));
      }

      // ignore tip button - any other pen button == erase
      const bool erasing = event.mButtons & ~1;

      const auto scale = mDrawings.at(pageIndex).mScale;

      const auto pressure = std::clamp(event.mPressure - 0.40f, 0.0f, 0.60f);

      auto ds = mKneeboard->GetDoodlesSettings();
      const auto tool = erasing ? ds.mEraser : ds.mPen;
      auto radius = tool.mMinimumRadius + (tool.mSensitivity * pressure);

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

ID2D1Bitmap* DoodleRenderer::GetDrawingSurface(PageIndex index) {
  if (index >= mDrawings.size()) {
    // Should have been initialized by cursor events
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  auto& page = mDrawings.at(index);
  if (page.mBitmap) {
    return page.mBitmap.get();
  }

  const auto& contentPixels = page.mNativeSize;
  if (contentPixels.height == 0 || contentPixels.width == 0) {
    OPENKNEEBOARD_BREAK;
    return nullptr;
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

  auto lock = mDXR.AcquireLock();
  winrt::com_ptr<ID3D11Texture2D> texture;
  winrt::check_hresult(
    mDXR.mD3DDevice->CreateTexture2D(&textureDesc, nullptr, texture.put()));
  page.mSurface = texture.as<IDXGISurface>();

  mDXR.mD2DDeviceContext->CreateBitmapFromDxgiSurface(
    page.mSurface.get(), nullptr, page.mBitmap.put());

  return page.mBitmap.get();
}

void DoodleRenderer::Render(
  ID2D1DeviceContext* ctx,
  PageIndex pageIndex,
  const D2D1_RECT_F& rect) {
  FlushCursorEvents();

  if (pageIndex >= mDrawings.size()) {
    return;
  }
  auto& page = mDrawings.at(pageIndex);
  auto bitmap = page.mBitmap;
  if (!bitmap) {
    return;
  }

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->DrawBitmap(
    bitmap.get(), rect, 1.0f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
}

}// namespace OpenKneeboard
