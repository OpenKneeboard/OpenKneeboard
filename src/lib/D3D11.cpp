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
#include <OpenKneeboard/Shaders/DXBC/Sprite.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <shims/winrt/base.h>

#include <directxtk/CommonStates.h>

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

  mCommonStates = std::make_unique<DirectX::DX11::CommonStates>(device);

  namespace Sprite = OpenKneeboard::Shaders::DXBC::Sprite;
  winrt::check_hresult(device->CreatePixelShader(
    Sprite::PS.data(), Sprite::PS.size(), nullptr, mPixelShader.put()));
  winrt::check_hresult(device->CreateVertexShader(
    Sprite::VS.data(), Sprite::VS.size(), nullptr, mVertexShader.put()));

  D3D11_BUFFER_DESC uniformDesc {
    .ByteWidth = sizeof(ShaderData::Uniform),
    .Usage = D3D11_USAGE_DYNAMIC,
    .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
    .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
  };
  winrt::check_hresult(
    device->CreateBuffer(&uniformDesc, nullptr, mUniformBuffer.put()));

  std::array vertexMembers {
    D3D11_INPUT_ELEMENT_DESC {
      .SemanticName = "SV_Position",
      .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
      .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
    },
    D3D11_INPUT_ELEMENT_DESC {
      .SemanticName = "COLOR",
      .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
      .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
      .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
    },
    D3D11_INPUT_ELEMENT_DESC {
      .SemanticName = "TEXCOORD",
      .Format = DXGI_FORMAT_R32G32_FLOAT,
      .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
      .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
    },
    D3D11_INPUT_ELEMENT_DESC {
      .SemanticName = "TEXCOORD",
      .SemanticIndex = 1,
      .Format = DXGI_FORMAT_R32G32_FLOAT,
      .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
      .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
    },
    D3D11_INPUT_ELEMENT_DESC {
      .SemanticName = "TEXCOORD",
      .SemanticIndex = 2,
      .Format = DXGI_FORMAT_R32G32_FLOAT,
      .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
      .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
    },
  };
  winrt::check_hresult(device->CreateInputLayout(
    vertexMembers.data(),
    std::size(vertexMembers),
    Sprite::VS.data(),
    Sprite::VS.size(),
    mInputLayout.put()));

  D3D11_BUFFER_DESC vertexDesc {
    .ByteWidth = sizeof(ShaderData::Vertex) * 6,// Two triangles
    .Usage = D3D11_USAGE_DYNAMIC,
    .BindFlags = D3D11_BIND_VERTEX_BUFFER,
    .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
  };
  winrt::check_hresult(
    device->CreateBuffer(&vertexDesc, nullptr, mVertexBuffer.put()));
}

SpriteBatch::~SpriteBatch() {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::SpriteBatch::~SpriteBatch()");
  if (mTarget) [[unlikely]] {
    fatal(
      "Destroying SpriteBatch while frame in progress; did you call End()?");
  }
}

void SpriteBatch::Begin(ID3D11RenderTargetView* rtv, const PixelSize& rtvSize) {
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

  ID3D11Buffer* uniformBuffers[] {mUniformBuffer.get()};

  ID3D11Buffer* vertexBuffers[] {mVertexBuffer.get()};
  UINT vertexStrides[] = {sizeof(ShaderData::Vertex)};
  UINT vertexOffsets[] = {0};

  static_assert(std::size(vertexBuffers) == std::size(vertexStrides));
  static_assert(std::size(vertexBuffers) == std::size(vertexOffsets));

  auto ctx = mDeviceContext.get();
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ctx->IASetInputLayout(mInputLayout.get());
  ctx->IASetVertexBuffers(
    0, std::size(vertexBuffers), vertexBuffers, vertexStrides, vertexOffsets);

  ctx->VSSetConstantBuffers(0, std::size(uniformBuffers), uniformBuffers);
  ctx->VSSetShader(mVertexShader.get(), nullptr, 0);

  ctx->PSSetConstantBuffers(0, std::size(uniformBuffers), uniformBuffers);
  ctx->PSSetShader(mPixelShader.get(), nullptr, 0);
  ctx->PSSetShaderResources(0, 1, &nullsrv);
  ID3D11SamplerState* samplers[] {mCommonStates->LinearClamp()};
  ctx->PSSetSamplers(0, std::size(samplers), samplers);

  ctx->RSSetState(mCommonStates->CullCounterClockwise());
  ctx->RSSetViewports(1, &viewport);
  ctx->RSSetScissorRects(1, &scissorRect);

  ctx->OMSetRenderTargets(1, &rtv, nullptr);
  ctx->OMSetDepthStencilState(mCommonStates->DepthNone(), 0);
  ctx->OMSetBlendState(
    mCommonStates->AlphaBlend(), DirectX::Colors::White, ~0ui32);

  mTarget = rtv;
  mTargetDimensions = rtvSize.StaticCast<float, std::array<float, 2>>();
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

  std::array<float, 2> texClampTL;
  std::array<float, 2> texClampBR;

  {
    winrt::com_ptr<ID3D11Resource> resource;
    source->GetResource(resource.put());
    D3D11_TEXTURE2D_DESC desc {};
    winrt::com_ptr<ID3D11Texture2D> texture;
    winrt::check_hresult(resource->QueryInterface(texture.put()));
    texture->GetDesc(&desc);

    texClampTL = {
      (sourceRect.Left() + 0.5f) / desc.Width,
      (sourceRect.Top() + 0.5f) / desc.Height,
    };
    texClampBR = {
      (sourceRect.Right() - 0.5f) / desc.Width,
      (sourceRect.Bottom() - 0.5f) / desc.Height,
    };

    D3D11_MAPPED_SUBRESOURCE mapping {};
    winrt::check_hresult(mDeviceContext->Map(
      mUniformBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping));
    const auto uniform = reinterpret_cast<ShaderData::Uniform*>(mapping.pData);

    *uniform = ShaderData::Uniform {
      .mSourceDimensions = {
        static_cast<float>(desc.Width),
        static_cast<float>(desc.Height),
      },
      .mDestDimensions = { mTargetDimensions[0], mTargetDimensions[1] },
    };

    mDeviceContext->Unmap(mUniformBuffer.get(), 0);
  }

  {
    D3D11_MAPPED_SUBRESOURCE mapping {};
    winrt::check_hresult(mDeviceContext->Map(
      mVertexBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping));

    using TexCoord = decltype(ShaderData::Vertex::mTexCoord);
    const TexCoord srcTL = sourceRect.TopLeft().StaticCast<float, TexCoord>();
    const TexCoord srcBR
      = sourceRect.BottomRight().StaticCast<float, TexCoord>();
    const TexCoord srcBL {srcTL[0], srcBR[1]};
    const TexCoord srcTR {srcBR[0], srcTL[1]};

    using Position = decltype(ShaderData::Vertex::mPosition);
    const Position dstTL {destRect.Left<float>(), destRect.Top<float>(), 0, 1};
    const Position dstBR {
      destRect.Right<float>(), destRect.Bottom<float>(), 0, 1};
    const Position dstBL {
      destRect.Left<float>(), destRect.Bottom<float>(), 0, 1};
    const Position dstTR {destRect.Right<float>(), destRect.Top<float>(), 0, 1};

    const auto makeVertex = [=](const auto& src, const auto& dst) {
      return ShaderData::Vertex {
        .mPosition = dst,
        .mColor = tint,
        .mTexCoord = src,
        .mTexClampTL = texClampTL,
        .mTexClampBR = texClampBR,
      };
    };

    const auto vertices = std::array {
      // First triangle: excludes top right
      makeVertex(srcBL, dstBL),
      makeVertex(srcTL, dstTL),
      makeVertex(srcBR, dstBR),
      // Second triangle: excludes bottom left
      makeVertex(srcTL, dstTL),
      makeVertex(srcTR, dstTR),
      makeVertex(srcBR, dstBR),
    };
    memcpy(mapping.pData, vertices.data(), sizeof(ShaderData::Vertex) * 6);

    mDeviceContext->Unmap(mVertexBuffer.get(), 0);
  }

  ID3D11ShaderResourceView* resources[] {source};
  mDeviceContext->PSSetShaderResources(0, std::size(resources), resources);
  mDeviceContext->Draw(6, 0);
}

void SpriteBatch::End() {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::SpriteBatch::End()");
  if (!mTarget) [[unlikely]] {
    fatal("target not set; double-End() or Begin() not called?");
  }

  ID3D11RenderTargetView* nullrtv {nullptr};
  mDeviceContext->OMSetRenderTargets(1, &nullrtv, nullptr);
  mTarget = nullptr;
}

}// namespace OpenKneeboard::D3D11
