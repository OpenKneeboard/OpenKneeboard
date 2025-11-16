// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/DoodleRenderer.hpp>
#include <OpenKneeboard/KneeboardState.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>

#include <mutex>

namespace OpenKneeboard {

DoodleRenderer::DoodleRenderer(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs)
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
  for (auto it = mDrawings.begin(); it != mDrawings.end(); /* no increment */) {
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
  if (!pageID) {
    return false;
  }
  auto it = mDrawings.find(pageID);
  if (it == mDrawings.end()) {
    return false;
  }
  return static_cast<bool>(it->second.mBitmap);
}

void DoodleRenderer::PostCursorEvent(
  KneeboardViewID,
  const CursorEvent& event,
  PageID pageID,
  const PixelSize& nativePageSize) {
  if (!nativePageSize) {
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
      if (event.mTouchState != CursorTouchState::TouchingSurface) {
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
  if (!contentPixels) {
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  const auto surfaceSize = contentPixels.ScaledToFit(MaxViewRenderSize);
  if (surfaceSize.IsEmpty()) [[unlikely]] {
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  page.mScale = surfaceSize.Height<float>() / contentPixels.Height();

  D3D11_TEXTURE2D_DESC textureDesc {
    .Width = surfaceSize.mWidth,
    .Height = surfaceSize.mHeight,
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
    mDXR->mD3D11Device->CreateTexture2D(&textureDesc, nullptr, texture.put()));
  page.mSurface = texture.as<IDXGISurface>();

  mDXR->mD2DDeviceContext->CreateBitmapFromDxgiSurface(
    page.mSurface.get(), nullptr, page.mBitmap.put());

  evAddedPageEvent.Emit();

  return page.mBitmap.get();
}

void DoodleRenderer::Render(
  ID2D1DeviceContext* ctx,
  PageID pageID,
  const PixelRect& rect) {
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

void DoodleRenderer::Render(
  RenderTarget* rt,
  PageID pageID,
  const PixelRect& rect) {
  FlushCursorEvents();

  if (!HaveDoodles(pageID)) {
    return;
  }

  this->Render(rt->d2d(), pageID, rect);
}

}// namespace OpenKneeboard
