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

#include <mutex>

namespace OpenKneeboard {

DoodleRenderer::DoodleRenderer(const std::shared_ptr<DXResources>& dxr, KneeboardState* kbs)
  : mDXR(dxr), mKneeboard(kbs) {
  mBrush = dxr->mBlackBrush;
  mEraser = dxr->mEraserBrush;
  mDrawingContext = mDXR->mD2DBackBufferDeviceContext;
}

DoodleRenderer::~DoodleRenderer() = default;

void DoodleRenderer::Clear() {
  mDrawings.clear();
}

void DoodleRenderer::ClearPage(PageID pageID) {
  mDrawings.erase(pageID);
}

void DoodleRenderer::ClearExcept(const std::unordered_set<PageID>& keep) {
  for (auto it = mDrawings.begin(); it != mDrawings.end();) {
    if (keep.contains(it->first)) {
      it++;
    } else {
      it = mDrawings.erase(it);
    }
  }
}

bool DoodleRenderer::HaveDoodles() const {
  for (const auto& [id, drawing]: mDrawings) {
    if (drawing.mBitmap) {
      return true;
    }
  }
  return false;
}

bool DoodleRenderer::HaveDoodles(PageID pageID) const {
  auto it = mDrawings.find(pageID);
  if (it == mDrawings.end()) {
    return false;
  }
  return static_cast<bool>(it->second.mBitmap);
}

void DoodleRenderer::PostCursorEvent(
  EventContext,
  const CursorEvent& event,
  PageID pageID,
  const D2D1_SIZE_U& nativePageSize) {
  if (nativePageSize.width == 0 || nativePageSize.height == 0) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  {
    std::scoped_lock lock(mBufferedEventsMutex);
    auto& drawing = mDrawings[pageID];
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

  for (auto& [pageID, page]: mDrawings) {
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
        ctx->SetTarget(GetDrawingSurface(pageID));
      }

      // ignore tip button - any other pen button == erase
      const bool erasing = event.mButtons & ~1;

      const auto scale = page.mScale;

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

ID2D1Bitmap* DoodleRenderer::GetDrawingSurface(PageID pageID) {
  auto it = mDrawings.find(pageID);
  if (it == mDrawings.end()) {
    // Should have been initialized by cursor events
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  auto& page = it->second;
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
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    .SampleDesc = {1, 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
  };

  const std::unique_lock lock(*mDXR);
  winrt::com_ptr<ID3D11Texture2D> texture;
  winrt::check_hresult(
    mDXR->mD3DDevice->CreateTexture2D(&textureDesc, nullptr, texture.put()));
  page.mSurface = texture.as<IDXGISurface>();

  mDXR->mD2DDeviceContext->CreateBitmapFromDxgiSurface(
    page.mSurface.get(), nullptr, page.mBitmap.put());

  evAddedPageEvent.Emit();

  return page.mBitmap.get();
}

void DoodleRenderer::Render(
  ID2D1DeviceContext* ctx,
  PageID pageID,
  const D2D1_RECT_F& rect) {
  FlushCursorEvents();

  auto it = mDrawings.find(pageID);
  if (it == mDrawings.end()) {
    return;
  }
  auto& page = it->second;

  auto bitmap = page.mBitmap;
  if (!bitmap) {
    return;
  }

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->DrawBitmap(
    bitmap.get(), rect, 1.0f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
}

}// namespace OpenKneeboard
