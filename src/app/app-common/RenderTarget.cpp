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

#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/RenderTarget.h>
#include <OpenKneeboard/SHM.h>

#include <OpenKneeboard/dprint.h>

#include <d2d1_3.h>

namespace OpenKneeboard {

std::shared_ptr<RenderTarget> RenderTarget::Create(
  const DXResources& dxr,
  const winrt::com_ptr<ID3D11Texture2D>& texture) {
  return std::shared_ptr<RenderTarget>(new RenderTarget(dxr, texture));
}

RenderTarget::~RenderTarget() = default;

RenderTarget::RenderTarget(
  const DXResources& dxr,
  const winrt::com_ptr<ID3D11Texture2D>& texture)
  : mDXR(dxr), mD3DTexture(texture) {
  winrt::check_hresult(dxr.mD3DDevice->CreateRenderTargetView(
    texture.get(), nullptr, mD3DRenderTargetView.put()));

  winrt::check_hresult(dxr.mD2DDeviceContext->CreateBitmapFromDxgiSurface(
    texture.as<IDXGISurface>().get(), nullptr, mD2DBitmap.put()));

  D3D11_TEXTURE2D_DESC desc;
  texture->GetDesc(&desc);
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  dxr.mD3DDevice->CreateTexture2D(
    &desc, nullptr, mD2DIntermediateD3DTexture.put());
  winrt::com_ptr<ID2D1ColorContext1> srgb, scrgb;
  auto d2d5 = dxr.mD2DDeviceContext.as<ID2D1DeviceContext5>();
  d2d5->CreateColorContextFromDxgiColorSpace(
    DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, srgb.put());
  d2d5->CreateColorContextFromDxgiColorSpace(
    DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709, scrgb.put());

  D2D1_BITMAP_PROPERTIES1 d2dProps {
    .pixelFormat = {desc.Format, D2D1_ALPHA_MODE_PREMULTIPLIED},
    .bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET,
    .colorContext = srgb.get(),
  };
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateBitmapFromDxgiSurface(
    mD2DIntermediateD3DTexture.as<IDXGISurface>().get(),
    &d2dProps,
    mD2DIntermediate.put()));
  winrt::check_hresult(dxr.mD3DDevice->CreateRenderTargetView(
    mD2DIntermediateD3DTexture.get(),
    nullptr,
    mD2DIntermediateD3DRenderTargetView.put()));

  winrt::check_hresult(dxr.mD2DDeviceContext->CreateEffect(
    CLSID_D2D1ColorManagement, mD2DColorManagement.put()));
  mD2DColorManagement->SetInput(0, mD2DIntermediate.get(), true);
  mD2DColorManagement->SetValue(
    D2D1_COLORMANAGEMENT_PROP_SOURCE_COLOR_CONTEXT, srgb.get());
  mD2DColorManagement->SetValue(
    D2D1_COLORMANAGEMENT_PROP_DESTINATION_COLOR_CONTEXT, scrgb.get());

  winrt::check_hresult(dxr.mD2DDeviceContext->CreateEffect(
    CLSID_D2D1WhiteLevelAdjustment, mD2DWhiteLevel.put()));
  mD2DWhiteLevel->SetInputEffect(0, mD2DColorManagement.get(), true);

  mD2DLastEffect = mD2DWhiteLevel.get();
}

RenderTargetID RenderTarget::GetID() const {
  return mID;
}

RenderTarget::D2D RenderTarget::d2d() {
  return {this->shared_from_this()};
}

RenderTarget::D3D RenderTarget::d3d() {
  return {this->shared_from_this()};
}

RenderTarget::D2D::D2D(const std::shared_ptr<RenderTarget>& parent)
  : mParent(parent) {
  if (!parent) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  mUnsafeParent = mParent.get();
  this->Acquire();
}

void RenderTarget::D2D::Acquire() {
  auto& mode = mParent->mMode;
  if (mode != Mode::Unattached) {
    mParent = nullptr;
    dprintf(
      "Attempted to activate D2D for a render target in mode {}",
      static_cast<int>(mode));
    OPENKNEEBOARD_BREAK;
    return;
  }

  mode = Mode::D2D;

  auto& hdr = *mUnsafeParent->mDXR.mHDRData;
  mHDR = hdr.mIsValid;
  if (mHDR) {
    winrt::check_hresult(mUnsafeParent->mD2DWhiteLevel->SetValue(
      D2D1_WHITELEVELADJUSTMENT_PROP_INPUT_WHITE_LEVEL,
      hdr.mSDRWhiteLevelInNits));
    winrt::check_hresult(mUnsafeParent->mD2DWhiteLevel->SetValue(
      D2D1_WHITELEVELADJUSTMENT_PROP_OUTPUT_WHITE_LEVEL,
      D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL));
  }

  (*this)->SetTarget(mUnsafeParent->mD2DIntermediate.get());
  mUnsafeParent->mDXR.PushD2DDraw();
  (*this)->SetTransform(D2D1::Matrix3x2F::Identity());
  (*this)->Clear({0.0f, 0.0f, 0.0f, 0.0f});
}

void RenderTarget::D2D::Release() {
  if (!mParent) {
    return;
  }
  if (mReleased) {
    dprintf("{}: double-release", __FUNCTION__);
    OPENKNEEBOARD_BREAK;
    return;
  }
  mReleased = true;

  (*this)->Flush();
  mUnsafeParent->mDXR.PopD2DDraw();
  mUnsafeParent->mDXR.mD2DDeviceContext->SetTarget(
    mUnsafeParent->mD2DBitmap.get());
  mUnsafeParent->mDXR.PushD2DDraw();
  (*this)->SetTransform(D2D1::Matrix3x2F::Identity());

  D3D11::BlockingFlush(mUnsafeParent->mDXR.mD3DImmediateContext);
  if (mHDR) {
    (*this)->DrawImage(mUnsafeParent->mD2DLastEffect);
  } else {
    (*this)->DrawImage(mUnsafeParent->mD2DIntermediate.get());
  }
  (*this)->Flush();
  mUnsafeParent->mDXR.PopD2DDraw();

  mUnsafeParent->mDXR.mD2DDeviceContext->SetTarget(nullptr);

  auto& mode = mParent->mMode;
  if (mode != Mode::D2D) {
    dprintf(
      "{}: attempting to release D2D, but mode is {}",
      __FUNCTION__,
      static_cast<int>(mode));
    OPENKNEEBOARD_BREAK;
    return;
  }

  mode = Mode::Unattached;
}

void RenderTarget::D2D::Reacquire() {
  if (!mReleased) {
    dprint("Attempting to re-acquire without release");
    OPENKNEEBOARD_BREAK;
    return;
  }
  this->Acquire();
  mReleased = false;
}

RenderTarget::D2D::~D2D() {
  if (mReleased || !mParent) {
    return;
  }
  this->Release();
}

ID2D1DeviceContext* RenderTarget::D2D::operator->() const {
  return mUnsafeParent->mDXR.mD2DDeviceContext.get();
}

RenderTarget::D2D::operator ID2D1DeviceContext*() const {
  return mUnsafeParent->mDXR.mD2DDeviceContext.get();
}

RenderTarget::D3D::D3D(const std::shared_ptr<RenderTarget>& parent)
  : mParent(parent) {
  if (!parent) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  mUnsafeParent = parent.get();

  auto& mode = mParent->mMode;
  if (mode != Mode::Unattached) {
    mParent = nullptr;
    dprintf(
      "Attempted to activate D3D for a render target in mode {}",
      static_cast<int>(mode));
    OPENKNEEBOARD_BREAK;
    return;
  }
  mode = Mode::D3D;
}

RenderTarget::D3D::~D3D() {
  if (!mParent) {
    return;
  }

  auto& mode = mParent->mMode;
  if (mode != Mode::D3D) {
    dprintf(
      "Attempting to leave D3D for render target in mode {}",
      static_cast<int>(mode));
    OPENKNEEBOARD_BREAK;
    return;
  }
  mode = Mode::Unattached;
}

ID3D11Texture2D* RenderTarget::D3D::texture() const {
  return mUnsafeParent->mD3DTexture.get();
}

ID3D11RenderTargetView* RenderTarget::D3D::rtv() const {
  return mUnsafeParent->mD3DRenderTargetView.get();
}

}// namespace OpenKneeboard