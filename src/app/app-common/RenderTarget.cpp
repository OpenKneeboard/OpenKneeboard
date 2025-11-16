// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include <OpenKneeboard/D3D11.hpp>
#include <OpenKneeboard/RenderTarget.hpp>
#include <OpenKneeboard/SHM.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <d2d1_3.h>

namespace OpenKneeboard {

std::shared_ptr<RenderTarget> RenderTarget::Create(
  const audited_ptr<DXResources>& dxr,
  const winrt::com_ptr<ID3D11Texture2D>& texture) {
  return std::shared_ptr<RenderTarget>(new RenderTarget(dxr, texture));
}

std::shared_ptr<RenderTarget> RenderTarget::Create(
  const audited_ptr<DXResources>& dxr,
  nullptr_t texture) {
  return std::shared_ptr<RenderTarget>(new RenderTarget(dxr, nullptr));
}

RenderTarget::~RenderTarget() = default;

RenderTarget::RenderTarget(
  const audited_ptr<DXResources>& dxr,
  const winrt::com_ptr<ID3D11Texture2D>& texture)
  : mDXR(dxr) {
  if (texture) {
    this->SetD3DTexture(texture);
  }
}

void RenderTarget::SetD3DTexture(
  const winrt::com_ptr<ID3D11Texture2D>& texture) {
  if (texture == mD3DTexture) {
    return;
  }
  mD2DBitmap = nullptr;
  mD3DRenderTargetView = nullptr;
  mD3DTexture = texture;

  if (!texture) {
    return;
  }

  winrt::check_hresult(mDXR->mD3D11Device->CreateRenderTargetView(
    texture.get(), nullptr, mD3DRenderTargetView.put()));

  winrt::check_hresult(mDXR->mD2DDeviceContext->CreateBitmapFromDxgiSurface(
    texture.as<IDXGISurface>().get(), nullptr, mD2DBitmap.put()));

  D3D11_TEXTURE2D_DESC desc;
  texture->GetDesc(&desc);
  mDimensions = {desc.Width, desc.Height};
}

PixelSize RenderTarget::GetDimensions() const {
  return mDimensions;
}

RenderTargetID RenderTarget::GetID() const {
  return mID;
}

RenderTarget::D2D RenderTarget::d2d(const std::source_location& loc) {
  if (!mD3DTexture) {
    fatal(loc, "Attempted to start D2D without a texture");
  }
  return {this->shared_from_this(), loc};
}

RenderTarget::D3D RenderTarget::d3d(const std::source_location& loc) {
  if (!mD3DTexture) {
    fatal(loc, "Attempted to start D3D without a texture");
  }
  return {this->shared_from_this()};
}

std::shared_ptr<RenderTargetWithMultipleIdentities>
RenderTargetWithMultipleIdentities::Create(
  const audited_ptr<DXResources>& dxr,
  const winrt::com_ptr<ID3D11Texture2D>& texture,
  size_t identityCount) {
  if (identityCount < 1) {
    fatal("Can't create a RenderTarget with no identities");
  }
  auto ret = std::shared_ptr<RenderTargetWithMultipleIdentities>(
    new RenderTargetWithMultipleIdentities(dxr, texture));
  ret->mIdentities.resize(identityCount);
  return ret;
}

RenderTargetID RenderTargetWithMultipleIdentities::GetID() const {
  return mIdentities.at(mCurrentIdentity);
}

void RenderTargetWithMultipleIdentities::SetActiveIdentity(size_t index) {
  if (index >= mIdentities.size()) [[unlikely]] {
    fatal("identity index out of bounds");
  }
  mCurrentIdentity = index;
}

RenderTarget::D2D::D2D(
  const std::shared_ptr<RenderTarget>& parent,
  const std::source_location& loc)
  : mParent(parent), mSourceLocation(loc) {
  if (!parent) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  mUnsafeParent = mParent.get();
  this->Acquire();
}

RenderTarget::D2D::D2D(D2D&& other) noexcept {
  mReleased = other.mReleased;
  mParent = std::move(other.mParent);
  mUnsafeParent = other.mUnsafeParent;
  mSourceLocation = std::move(other.mSourceLocation);
  mHDR = other.mHDR;

  other.mUnsafeParent = nullptr;
  other.mReleased = true;
}

void RenderTarget::D2D::Acquire() {
  mParent->mState.Transition<State::Unattached, State::D2D>();

  (*this)->SetTarget(mUnsafeParent->mD2DBitmap.get());
  mUnsafeParent->mDXR->PushD2DDraw(mSourceLocation);
  (*this)->SetTransform(D2D1::Matrix3x2F::Identity());
}

void RenderTarget::D2D::Release() {
  if (!mParent) {
    return;
  }
  if (mReleased) {
    dprint("{}: double-release", __FUNCTION__);
    OPENKNEEBOARD_BREAK;
    return;
  }
  mReleased = true;
  mUnsafeParent->mDXR->PopD2DDraw();
  mUnsafeParent->mDXR->mD2DDeviceContext->SetTarget(nullptr);

  mUnsafeParent->mState.Transition<State::D2D, State::Unattached>();
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
  return mUnsafeParent->mDXR->mD2DDeviceContext.get();
}

RenderTarget::D2D::operator ID2D1DeviceContext*() const {
  return mUnsafeParent->mDXR->mD2DDeviceContext.get();
}

RenderTarget::D3D::D3D(const std::shared_ptr<RenderTarget>& parent)
  : mParent(parent) {
  if (!parent) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  mUnsafeParent = parent.get();

  mParent->mState.Transition<State::Unattached, State::D3D>();
}

RenderTarget::D3D::~D3D() {
  if (!mParent) {
    return;
  }

  mParent->mState.Transition<State::D3D, State::Unattached>();
}

ID3D11Texture2D* RenderTarget::D3D::texture() const {
  return mUnsafeParent->mD3DTexture.get();
}

ID3D11RenderTargetView* RenderTarget::D3D::rtv() const {
  return mUnsafeParent->mD3DRenderTargetView.get();
}

}// namespace OpenKneeboard