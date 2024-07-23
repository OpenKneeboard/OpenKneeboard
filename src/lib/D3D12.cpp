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
#include <OpenKneeboard/D3D12.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <directxtk12/RenderTargetState.h>
#include <directxtk12/ResourceUploadBatch.h>

namespace OpenKneeboard::D3D12 {

SpriteBatch::SpriteBatch(
  ID3D12Device* device,
  ID3D12CommandQueue* queue,
  DXGI_FORMAT destFormat) {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::SpriteBatch()");

  mDevice.copy_from(device);

  {
    OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::ResourceUpload()");
    DirectX::ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();
    DirectX::RenderTargetState rtState(destFormat, DXGI_FORMAT_UNKNOWN);
    DirectX::SpriteBatchPipelineStateDescription pd(rtState);
    {
      OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::DXTKSpriteBatch()");

      mDXTKSpriteBatch
        = std::make_unique<DirectX::SpriteBatch>(device, resourceUpload, pd);
    }
    auto uploadResourcesFinished = resourceUpload.End(queue);
    uploadResourcesFinished.wait();
  }
}

SpriteBatch::~SpriteBatch() {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::~SpriteBatch()");

  if (mCommandList) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL(
      "Destroying while frame in progress; did you call End()?");
  }
}

void SpriteBatch::Begin(
  ID3D12GraphicsCommandList* commandList,
  const D3D12_CPU_DESCRIPTOR_HANDLE& renderTargetView,
  const PixelSize& rtvSize) {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::Begin()");
  if (mCommandList) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL(
      "frame already in progress; did you call End()?");
  }

  mCommandList = commandList;
  mRenderTargetView = renderTargetView;
  mRenderTargetViewSize = rtvSize;

  const D3D12_VIEWPORT viewport {
    0,
    0,
    rtvSize.Width<FLOAT>(),
    rtvSize.Height<FLOAT>(),
    0,
    1,
  };
  const D3D12_RECT scissorRect {
    0,
    0,
    rtvSize.Width<LONG>(),
    rtvSize.Height<LONG>(),
  };

  const auto ctx = commandList;
  ctx->RSSetViewports(1, &viewport);
  ctx->RSSetScissorRects(1, &scissorRect);
  ctx->OMSetRenderTargets(1, &renderTargetView, false, nullptr);

  mDXTKSpriteBatch->SetViewport(viewport);
  mDXTKSpriteBatch->Begin(commandList);
}

void SpriteBatch::Clear(DirectX::XMVECTORF32 color) {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::Clear()");
  if (!mCommandList) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL("target not set, call BeginFrame()");
  }

  mCommandList->ClearRenderTargetView(mRenderTargetView, color, 0, nullptr);
}

void SpriteBatch::Draw(
  const D3D12_GPU_DESCRIPTOR_HANDLE& sourceShaderResourceView,
  const PixelSize& sourceSize,
  const PixelRect& sourceRect,
  const PixelRect& destRect,
  const DirectX::XMVECTORF32 tint) {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::Draw()");
  if (!mCommandList) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL("target not set, call BeginFrame()");
  }

  const auto sourceD3DSize
    = sourceSize.StaticCast<uint32_t, DirectX::XMUINT2>();

  const D3D12_RECT sourceD3DRect = sourceRect;
  const D3D12_RECT destD3DRect = destRect;

  mDXTKSpriteBatch->Draw(
    sourceShaderResourceView, sourceD3DSize, destD3DRect, &sourceD3DRect, tint);
}

void SpriteBatch::End() {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::End()");
  if (!mCommandList) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL(
      "target not set; double-End() or Begin() not called?");
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("Impl()");
    mDXTKSpriteBatch->End();
  }

  mCommandList = nullptr;
  mRenderTargetView = {};
  mRenderTargetViewSize = {};
}

}// namespace OpenKneeboard::D3D12