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
#include <OpenKneeboard/SHM.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/tracing.h>

#include <shims/winrt/base.h>

#include <directxtk/SpriteBatch.h>

#include <d3d11_3.h>

namespace OpenKneeboard::D3D11 {

struct SavedState::Impl {
  winrt::com_ptr<ID3D11DeviceContext1> mContext;
  winrt::com_ptr<ID3DDeviceContextState> mState;
  static thread_local bool tHaveSavedState;
};

thread_local bool SavedState::Impl::tHaveSavedState {false};

SavedState::SavedState(const winrt::com_ptr<ID3D11DeviceContext>& ctx) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::SavedState::SavedState()");
  if (Impl::tHaveSavedState) {
    dprintf("`{}`: nesting D3D11 SavedStates", __FUNCSIG__);
    abort();
  }

  Impl::tHaveSavedState = true;
  mImpl = new Impl {.mContext = ctx.as<ID3D11DeviceContext1>()};

  winrt::com_ptr<ID3D11Device> device;
  ctx->GetDevice(device.put());
  auto featureLevel = device->GetFeatureLevel();

  winrt::com_ptr<ID3DDeviceContextState> newState;
  {
    OPENKNEEBOARD_TraceLoggingScope("CreateDeviceContextState");
    winrt::check_hresult(device.as<ID3D11Device1>()->CreateDeviceContextState(
      (device->GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED)
        ? D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED
        : 0,
      &featureLevel,
      1,
      D3D11_SDK_VERSION,
      __uuidof(ID3D11Device),
      nullptr,
      newState.put()));
  }
  {
    OPENKNEEBOARD_TraceLoggingScope("SwapDeviceContextState");
    mImpl->mContext->SwapDeviceContextState(
      newState.get(), mImpl->mState.put());
  }
}

SavedState::~SavedState() {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::SavedState::~SavedState()");
  mImpl->mContext->SwapDeviceContextState(mImpl->mState.get(), nullptr);
  Impl::tHaveSavedState = false;
  delete mImpl;
}

void CopyTextureWithTint(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  DirectX::FXMVECTOR tint) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "D3D11::CopyTextureWithTint");
  winrt::com_ptr<ID3D11DeviceContext> ctx;
  device->GetImmediateContext(ctx.put());

  ctx->ClearRenderTargetView(dest, DirectX::Colors::Transparent);
  D3D11_VIEWPORT viewport {
    0.0f,
    0.0f,
    static_cast<FLOAT>(TextureWidth),
    static_cast<FLOAT>(TextureHeight),
    0.0f,
    1.0f,
  };
  ctx->RSSetViewports(1, &viewport);
  ctx->OMSetDepthStencilState(nullptr, 0);
  ctx->OMSetRenderTargets(1, &dest, nullptr);
  ctx->OMSetBlendState(nullptr, nullptr, ~static_cast<UINT>(0));
  ctx->IASetInputLayout(nullptr);
  ctx->VSSetShader(nullptr, nullptr, 0);

  {
    TraceLoggingThreadActivity<gTraceProvider> subActivity;
    TraceLoggingWriteStart(subActivity, "SpriteBatch");
    DirectX::SpriteBatch sprites(ctx.get());
    sprites.Begin();
    sprites.Draw(source, DirectX::XMFLOAT2 {0.0f, 0.0f}, tint);
    sprites.End();
    TraceLoggingWriteStop(subActivity, "SpriteBatch");
  }

  TraceLoggingWriteStop(activity, "D3D11::CopyTextureWithTint");
}

void DrawTextureWithTint(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  const RECT& sourceRect,
  const RECT& destRect,
  DirectX::FXMVECTOR tint) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "D3D11::DrawTextureWithTint");
  winrt::com_ptr<ID3D11DeviceContext> ctx;
  device->GetImmediateContext(ctx.put());

  D3D11_VIEWPORT viewport {
    0.0f,
    0.0f,
    static_cast<FLOAT>(destRect.right),
    static_cast<FLOAT>(destRect.bottom),
    0.0f,
    1.0f,
  };

  ctx->RSSetViewports(1, &viewport);
  ctx->OMSetDepthStencilState(nullptr, 0);
  ctx->OMSetRenderTargets(1, &dest, nullptr);
  ctx->OMSetBlendState(nullptr, nullptr, ~static_cast<UINT>(0));
  ctx->IASetInputLayout(nullptr);
  ctx->VSSetShader(nullptr, nullptr, 0);

  {
    TraceLoggingThreadActivity<gTraceProvider> subActivity;
    TraceLoggingWriteStart(subActivity, "SpriteBatch");
    DirectX::SpriteBatch sprites(ctx.get());
    sprites.Begin();
    sprites.Draw(source, destRect, &sourceRect, tint);
    sprites.End();
    TraceLoggingWriteStop(subActivity, "SpriteBatch");
  }

  TraceLoggingWriteStop(activity, "D3D11::DrawTextureWithTint");
}

void CopyTextureWithOpacity(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  float opacity) {
  CopyTextureWithTint(
    device,
    source,
    dest,
    DirectX::FXMVECTOR {opacity, opacity, opacity, opacity});
}

void DrawTextureWithOpacity(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  const RECT& sourceRect,
  const RECT& destRect,
  float opacity) {
  DrawTextureWithTint(
    device,
    source,
    dest,
    sourceRect,
    destRect,
    DirectX::FXMVECTOR {opacity, opacity, opacity, opacity});
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
    dprintf(
      "`{}`: destroying SpriteBatch while frame in progress", __FUNCSIG__);
    abort();
  }
}

void SpriteBatch::Begin(ID3D11RenderTargetView* rtv, const PixelSize& rtvSize) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::SpriteBatch::Begin()");
  if (mTarget) [[unlikely]] {
    dprintf("`{}`: frame already in progress", __FUNCSIG__);
    abort();
  }
  const D3D11_VIEWPORT viewport {
    0,
    0,
    rtvSize.GetWidth<FLOAT>(),
    rtvSize.GetHeight<FLOAT>(),
    0,
    1,
  };

  const D3D11_RECT scissorRect {
    0,
    0,
    rtvSize.GetWidth<LONG>(),
    rtvSize.GetHeight<LONG>(),
  };

  auto ctx = mDeviceContext.get();
  ctx->IASetInputLayout(nullptr);
  ctx->VSSetShader(nullptr, nullptr, 0);
  ctx->RSSetViewports(1, &viewport);
  ctx->RSSetScissorRects(1, &scissorRect);
  ctx->OMSetRenderTargets(1, &rtv, nullptr);
  ctx->OMSetDepthStencilState(nullptr, 0);
  ctx->OMSetBlendState(nullptr, nullptr, ~static_cast<UINT>(0));

  mDXTKSpriteBatch->Begin();

  mTarget = rtv;
}

void SpriteBatch::Clear(DirectX::XMVECTORF32 color) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::SpriteBatch::Clear()");
  if (!mTarget) [[unlikely]] {
    dprintf("`{}`: target not set, call BeginFrame()", __FUNCSIG__);
    abort();
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
    dprintf("`{}`: target not set, call BeginFrame()", __FUNCSIG__);
    abort();
  }

  const D3D11_RECT sourceD3DRect = sourceRect;
  const D3D11_RECT destD3DRect = destRect;

  mDXTKSpriteBatch->Draw(source, destD3DRect, &sourceD3DRect, tint);
}

void SpriteBatch::End() {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::SpriteBatch::End()");
  if (!mTarget) [[unlikely]] {
    dprintf(
      "`{}`: target not set; double-End() or Begin() not called?", __FUNCSIG__);
    abort();
  }
  mDXTKSpriteBatch->End();
}

}// namespace OpenKneeboard::D3D11
