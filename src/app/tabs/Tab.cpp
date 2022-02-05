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
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

#include <OpenKneeboard/DXResources.h>
#include "okEvents.h"
#include "scope_guard.h"

namespace OpenKneeboard {

class Tab::Impl final {
 public:
  DXResources mDXR;
  std::string mTitle;

  winrt::com_ptr<ID2D1SolidColorBrush> mBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mEraser;
  bool mHaveCursor = false;
  D2D1_POINT_2F mCursorPoint;

  struct Drawing {
    winrt::com_ptr<IDXGISurface> mSurface;
    float mScale;
  };
  std::vector<Drawing> mDrawings;

  winrt::com_ptr<IDXGISurface> GetDrawingSurface(
    uint16_t index,
    const D2D1_SIZE_U& contentPixels);
};

Tab::Tab(const DXResources& dxr, const wxString& title)
  : p(std::make_unique<Impl>()) {
  p->mDXR = dxr;
  p->mTitle = title;

  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), p->mBrush.put()));
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(1.0f, 0.0f, 1.0f, 0.0f), p->mEraser.put()));
}

Tab::~Tab() {
}

std::string Tab::GetTitle() const {
  return p->mTitle;
}

void Tab::Reload() {
}

void Tab::OnGameEvent(const GameEvent&) {
}

void Tab::ClearDrawings() {
  p->mDrawings.clear();
}

wxWindow* Tab::GetSettingsUI(wxWindow* parent) {
  return nullptr;
}

nlohmann::json Tab::GetSettings() const {
  return {};
}

void Tab::OnCursorEvent(const CursorEvent& event, uint16_t pageIndex) {
  if (
    event.TouchState != CursorTouchState::TOUCHING_SURFACE
    || event.PositionState != CursorPositionState::IN_CLIENT_RECT) {
    p->mHaveCursor = false;
    return;
  }
  // ignore tip button - any other pen button == erase
  const bool erasing = event.buttons & ~1;

  auto surface
    = p->GetDrawingSurface(pageIndex, this->GetPreferredPixelSize(pageIndex));
  auto ctx = p->mDXR.mD2DDeviceContext;
  winrt::com_ptr<ID2D1Bitmap1> bitmap;
  ctx->CreateBitmapFromDxgiSurface(surface.get(), nullptr, bitmap.put());
  ctx->SetTarget(bitmap.get());

  const auto scale = p->mDrawings.at(pageIndex).mScale;
  const auto pressure
    = 0.10 + (std::clamp(event.pressure - 0.50, 0.0, 0.50) * 0.9);
  const auto radius = 20 * pressure * (erasing ? 10 : 1);
  const auto x = event.x * scale;
  const auto y = event.y * scale;
  const auto brush = erasing ? p->mEraser : p->mBrush;

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->BeginDraw();
  ctx->SetPrimitiveBlend(
    erasing ? D2D1_PRIMITIVE_BLEND_COPY : D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
  auto cleanup = scope_guard([&]() {
    ctx->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
    winrt::check_hresult(ctx->EndDraw());
  });

  if (p->mHaveCursor) {
    ctx->DrawLine(p->mCursorPoint, {x, y}, brush.get(), radius * 2);
  }
  ctx->FillEllipse(D2D1::Ellipse({x, y}, radius, radius), brush.get());
  p->mHaveCursor = true;
  p->mCursorPoint = {x, y};
}

winrt::com_ptr<IDXGISurface> Tab::Impl::GetDrawingSurface(
  uint16_t index,
  const D2D1_SIZE_U& contentPixels) {
  if (index >= mDrawings.size()) {
    mDrawings.resize(index + 1);
  }

  auto& page = mDrawings.at(index);
  if (page.mSurface) {
    return page.mSurface;
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
  return page.mSurface;
}

void Tab::RenderPage(
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  RenderPageContent(pageIndex, rect);

  if (pageIndex >= p->mDrawings.size()) {
    return;
  }

  auto& page = p->mDrawings.at(pageIndex);
  auto& surface = page.mSurface;

  if (!surface) {
    return;
  }

  auto ctx = p->mDXR.mD2DDeviceContext;
  winrt::com_ptr<ID2D1Bitmap1> bitmap;
  ctx->CreateBitmapFromDxgiSurface(surface.get(), nullptr, bitmap.put());

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->DrawBitmap(
    bitmap.get(), rect, 1.0f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
}

}// namespace OpenKneeboard
