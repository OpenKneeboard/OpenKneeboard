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
#include <OpenKneeboard/D3D11.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <d3d11_3.h>

namespace OpenKneeboard::D3D11 {

DeviceContextState::DeviceContextState(ID3D11Device1* device) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::DeviceContextState(ID3D11Device1*)");

  auto featureLevel = device->GetFeatureLevel();
  check_hresult(device->CreateDeviceContextState(
    (device->GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED)
      ? D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED
      : 0,
    &featureLevel,
    1,
    D3D11_SDK_VERSION,
    __uuidof(ID3D11Device),
    nullptr,
    mState.put()));
}

ScopedDeviceContextStateChange::ScopedDeviceContextStateChange(
  const winrt::com_ptr<ID3D11DeviceContext1>& context,
  DeviceContextState* newState)
  : mContext(context) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::ScopedDeviceContextStateChange()");
  if (!newState->IsValid()) {
    winrt::com_ptr<ID3D11Device> device;
    context->GetDevice(device.put());
    *newState = {device.as<ID3D11Device1>().get()};
  }
  context->SwapDeviceContextState(newState->Get(), mOriginalState.put());
}

ScopedDeviceContextStateChange::~ScopedDeviceContextStateChange() {
  OPENKNEEBOARD_TraceLoggingScope("~D3D11::ScopedDeviceContextStateChange()");
  mContext->SwapDeviceContextState(mOriginalState.get(), nullptr);
}

SpriteBatch::SpriteBatch(ID3D11Device* device) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::SpriteBatch::SpriteBatch()");
  mDevice.copy_from(device);
  device->GetImmediateContext(mDeviceContext.put());

  mDXTKSpriteBatch
    = std::make_unique<DirectX::SpriteBatch>(mDeviceContext.get());
}

SpriteBatch::~SpriteBatch() {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::SpriteBatch::~SpriteBatch()");
  if (mTarget) [[unlikely]] {
    fatal(
      "Destroying SpriteBatch while frame in progress; did you call End()?");
  }
}

void SpriteBatch::Begin(
  ID3D11RenderTargetView* rtv,
  const PixelSize& rtvSize,
  std::function<void __cdecl()> setCustomShaders) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::SpriteBatch::Begin()");
  if (mTarget) [[unlikely]] {
    fatal("frame already in progress; did you call End()?");
  }
  const D3D11_VIEWPORT viewport {
    0,
    0,
    rtvSize.Width<FLOAT>(),
    rtvSize.Height<FLOAT>(),
    0,
    1,
  };

  const D3D11_RECT scissorRect {
    0,
    0,
    rtvSize.Width<LONG>(),
    rtvSize.Height<LONG>(),
  };

  ID3D11ShaderResourceView* nullsrv {nullptr};

  auto ctx = mDeviceContext.get();
  ctx->IASetInputLayout(nullptr);
  ctx->VSSetShader(nullptr, nullptr, 0);
  ctx->RSSetViewports(1, &viewport);
  ctx->RSSetScissorRects(1, &scissorRect);
  ctx->PSSetShaderResources(0, 1, &nullsrv);
  ctx->OMSetRenderTargets(1, &rtv, nullptr);
  ctx->OMSetDepthStencilState(nullptr, 0);
  ctx->OMSetBlendState(nullptr, nullptr, ~static_cast<UINT>(0));

  mDXTKSpriteBatch->Begin(
    DirectX::DX11::SpriteSortMode_Deferred,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    setCustomShaders);

  mTarget = rtv;
}

void SpriteBatch::Clear(DirectX::XMVECTORF32 color) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::SpriteBatch::Clear()");
  if (!mTarget) [[unlikely]] {
    fatal("target not set, call BeginFrame()");
  }
  mDeviceContext->ClearRenderTargetView(mTarget, color);
};

void SpriteBatch::Draw(
  ID3D11ShaderResourceView* source,
  const PixelRect& sourceRect,
  const PixelRect& destRect,
  const DirectX::XMVECTORF32 tint) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::SpriteBatch::Draw()");
  if (!mTarget) [[unlikely]] {
    fatal("target not set, call BeginFrame()");
  }

  const D3D11_RECT sourceD3DRect = sourceRect;
  const D3D11_RECT destD3DRect = destRect;

  mDXTKSpriteBatch->Draw(source, destD3DRect, &sourceD3DRect, tint);
}

void SpriteBatch::End() {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::SpriteBatch::End()");
  if (!mTarget) [[unlikely]] {
    fatal("target not set; double-End() or Begin() not called?");
  }
  mDXTKSpriteBatch->End();
  ID3D11RenderTargetView* nullrtv {nullptr};
  mDeviceContext->OMSetRenderTargets(1, &nullrtv, nullptr);
  mTarget = nullptr;
}

}// namespace OpenKneeboard::D3D11
