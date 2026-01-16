// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include "OpenKneeboard/numeric_cast.hpp"

#include <OpenKneeboard/D3D12/SpriteBatch.hpp>
#include <OpenKneeboard/Shaders/SpriteBatch/DXIL.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/hresult.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <directxtk12/GraphicsMemory.h>

namespace OpenKneeboard::D3D12 {

SpriteBatch::SpriteBatch(
  ID3D12Device* device,
  ID3D12CommandQueue* queue,
  DXGI_FORMAT format)
  : mDevice(device), mCommandQueue(queue) {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::SpriteBatch()");
  constexpr D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags
    = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
    | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
    | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

  constexpr auto srvRanges = std::array {
    D3D12_DESCRIPTOR_RANGE1 {
      .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
      .NumDescriptors = Shaders::SpriteBatch::MaxSpritesPerBatch,
      .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
    },
  };

  const auto rootParameters = std::array {
    D3D12_ROOT_PARAMETER1 {
      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
      .Descriptor = D3D12_ROOT_DESCRIPTOR1 {
        .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC,
      },
      .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
    },
    D3D12_ROOT_PARAMETER1 {
      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
      .DescriptorTable = {srvRanges.size(), srvRanges.data()},
      .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
    },
  };

  constexpr D3D12_STATIC_SAMPLER_DESC samplerDesc {
    .Filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR,
    .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
    .MaxLOD = D3D12_FLOAT32_MAX,
    .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
  };

  const D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc {
    .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
    .Desc_1_1 = D3D12_ROOT_SIGNATURE_DESC1 {
      .NumParameters = rootParameters.size(),
      .pParameters = rootParameters.data(),
      .NumStaticSamplers = 1,
      .pStaticSamplers = &samplerDesc,
      .Flags = rootSignatureFlags,
    },
  };

  winrt::com_ptr<ID3DBlob> signature;
  winrt::com_ptr<ID3DBlob> error;
  const auto serializeResult = D3D12SerializeVersionedRootSignature(
    &rootDesc, signature.put(), error.put());
  if (!SUCCEEDED(serializeResult)) {
    if (error) {
      const std::string_view strError {
        reinterpret_cast<const char*>(error->GetBufferPointer()),
        error->GetBufferSize()};
      dprint.Error("Failed to serialize root signature: {}", strError);
    }
    fatal_with_hresult(serializeResult);
  }
  check_hresult(mDevice->CreateRootSignature(
    1,
    signature->GetBufferPointer(),
    signature->GetBufferSize(),
    IID_PPV_ARGS(mRootSignature.put())));
  check_hresult(mRootSignature->SetName(
    L"OpenKneeboard::D3D12::SpriteBatch::RootSignature"));

  constexpr auto inputElements = std::array {
    D3D12_INPUT_ELEMENT_DESC {
      "SV_Position",
      0,
      DXGI_FORMAT_R32G32B32A32_FLOAT,
      0,
      offsetof(Shaders::SpriteBatch::Vertex, mPosition),
      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
      0,
    },
    D3D12_INPUT_ELEMENT_DESC {
      "COLOR",
      0,
      DXGI_FORMAT_R32G32B32A32_FLOAT,
      0,
      offsetof(Shaders::SpriteBatch::Vertex, mColor),
      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
      0,
    },
    D3D12_INPUT_ELEMENT_DESC {
      "TEXCOORD",
      0,
      DXGI_FORMAT_R32G32_FLOAT,
      0,
      offsetof(Shaders::SpriteBatch::Vertex, mTexCoord),
      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
      0,
    },
    D3D12_INPUT_ELEMENT_DESC {
      "TEXTURE_INDEX",
      0,
      DXGI_FORMAT_R32_UINT,
      0,
      offsetof(Shaders::SpriteBatch::Vertex, mTextureIndex),
      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
      0,
    },
  };

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc {
    .pRootSignature = mRootSignature.get(),
    .VS = Shaders::SpriteBatch::DXIL::VS,
    .PS = Shaders::SpriteBatch::DXIL::PS,
    .SampleMask = UINT_MAX,
    .RasterizerState = {D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_BACK},
    .InputLayout = {inputElements.data(), inputElements.size()},
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .NumRenderTargets = 1,
    .SampleDesc = {1},
  };

  pipelineDesc.RTVFormats[0] = format;

  for (auto& desc: pipelineDesc.BlendState.RenderTarget) {
    desc = D3D12_RENDER_TARGET_BLEND_DESC {
      .BlendEnable = TRUE,
      .LogicOpEnable = FALSE,
      .SrcBlend = D3D12_BLEND_ONE,
      .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
      .BlendOp = D3D12_BLEND_OP_ADD,
      .SrcBlendAlpha = D3D12_BLEND_ONE,
      .DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
      .BlendOpAlpha = D3D12_BLEND_OP_ADD,
      .LogicOp = D3D12_LOGIC_OP_NOOP,
      .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
    };
  }

  check_hresult(mDevice->CreateGraphicsPipelineState(
    &pipelineDesc, IID_PPV_ARGS(mGraphicsPipeline.put())));

  mShaderResourceViewHeap = std::make_unique<DirectX::DescriptorHeap>(
    device,
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    MaxSpritesPerBatch * MaxInflightFrames);
  check_hresult(mShaderResourceViewHeap->Heap()->SetName(
    L"OpenKneeboard::D3D12::SpriteBatch::mShaderResourceViewHeap"));
}

SpriteBatch::~SpriteBatch() {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::~SpriteBatch()");
  if (mNextFrame) [[unlikely]] {
    fatal("Destroying while frame in progress; did you call End()?");
  }
}

void SpriteBatch::Begin(
  ID3D12GraphicsCommandList* commandList,
  const D3D12_CPU_DESCRIPTOR_HANDLE& renderTargetView,
  const PixelSize& rtvSize) {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::Begin()");
  if (mNextFrame) [[unlikely]] {
    fatal("frame already in progress; did you call End()?");
  }

  mNextFrame = NextFrame {
    .mCommandList = commandList,
    .mRenderTargetView = renderTargetView,
    .mRenderTargetViewSize = rtvSize,
  };

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
  ctx->SetGraphicsRootSignature(mRootSignature.get());
  ctx->SetPipelineState(mGraphicsPipeline.get());
  ctx->RSSetViewports(1, &viewport);
  ctx->RSSetScissorRects(1, &scissorRect);
  ctx->OMSetRenderTargets(1, &renderTargetView, false, nullptr);
}

void SpriteBatch::Clear(DirectX::XMVECTORF32 color) {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::Clear()");
  if (!mNextFrame) [[unlikely]] {
    fatal("target not set, call BeginFrame()");
  }

  mNextFrame->mCommandList->ClearRenderTargetView(
    mNextFrame->mRenderTargetView, color, 0, nullptr);
}

void SpriteBatch::Draw(
  ID3D12Resource* source,
  const PixelSize& sourceSize,
  const PixelRect& sourceRect,
  const PixelRect& destRect,
  const DirectX::XMVECTORF32 tint) {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::Draw()");
  if (!mNextFrame) [[unlikely]] {
    fatal("target not set, call Begin()");
  }

  mNextFrame->mSprites.push_back(
    Sprite {
      .mSource = source,
      .mSourceSize = sourceSize,
      .mSourceRect = sourceRect,
      .mDestRect = destRect,
      .mTint = tint,
    });
}

void SpriteBatch::End() {
  OPENKNEEBOARD_TraceLoggingScope("D3D12::SpriteBatch::End()");
  if (!mNextFrame) [[unlikely]] {
    fatal("target not set; double-End() or Begin() not called?");
  }
  if (mNextFrame->mSprites.empty()) [[unlikely]] {
    fatal("no sprites");
  }
  const scope_exit clearNext {[this]() { this->mNextFrame = std::nullopt; }};

  const auto& targetSize = mNextFrame->mRenderTargetViewSize;
  CBuffer constantData {
    .mTargetDimensions
    = {targetSize.Width<float>(), targetSize.Height<float>()},
  };

  std::vector<Vertex> vertices;
  vertices.reserve(mNextFrame->mSprites.size() * VerticesPerSprite);

  const auto heapOffset
    = (mDrawCount++ % MaxInflightFrames) * MaxSpritesPerBatch;

  for (uint32_t i = 0; i < mNextFrame->mSprites.size(); i++) {
    const auto& sprite = mNextFrame->mSprites[i];

    mDevice->CreateShaderResourceView(
      sprite.mSource,
      nullptr,
      mShaderResourceViewHeap->GetCpuHandle(
        numeric_cast<std::size_t>(heapOffset + i)));

    constantData.mSourceDimensions[i] = {
      sprite.mSourceSize.Width<float>(),
      sprite.mSourceSize.Height<float>(),
    };

    constantData.mSourceClamp[i] = {
      (sprite.mSourceRect.Left<float>() + 0.5f) / sprite.mSourceSize.Width(),
      (sprite.mSourceRect.Top<float>() + 0.5f) / sprite.mSourceSize.Height(),
      (sprite.mSourceRect.Right<float>() - 0.5f) / sprite.mSourceSize.Width(),
      (sprite.mSourceRect.Bottom<float>() - 0.5f) / sprite.mSourceSize.Height(),
    };

    using TexCoord = std::array<float, 2>;

    const TexCoord srcTL
      = sprite.mSourceRect.TopLeft().StaticCast<float, TexCoord>();
    const TexCoord srcBR
      = sprite.mSourceRect.BottomRight().StaticCast<float, TexCoord>();
    const TexCoord srcBL {srcTL[0], srcBR[1]};
    const TexCoord srcTR {srcBR[0], srcTL[1]};

    using Position = Vertex::Position;

    // Destination coordinates in real 3d coordinates
    const Position dstTL
      = sprite.mDestRect.TopLeft().StaticCast<float, Position>();
    const Position dstBR
      = sprite.mDestRect.BottomRight().StaticCast<float, Position>();
    const Position dstTR {dstBR[0], dstTL[1]};
    const Position dstBL {dstTL[0], dstBR[1]};

    auto makeVertex = [=](const TexCoord& tc, const Position& pos) {
      return Vertex {
        .mPosition = pos,
        .mColor = std::bit_cast<decltype(Vertex::mColor)>(sprite.mTint),
        .mTexCoord = tc,
        .mTextureIndex = i,
      };
    };

    // A rectangle is two triangles

    // First triangle: excludes top right
    vertices.push_back(makeVertex(srcBL, dstBL));
    vertices.push_back(makeVertex(srcTL, dstTL));
    vertices.push_back(makeVertex(srcBR, dstBR));

    // Second triangle: excludes bottom left
    vertices.push_back(makeVertex(srcTL, dstTL));
    vertices.push_back(makeVertex(srcTR, dstTR));
    vertices.push_back(makeVertex(srcBR, dstBR));
  }

  auto& graphicsMemory = DirectX::DX12::GraphicsMemory::Get(mDevice);
  const auto constantBuffer = graphicsMemory.AllocateConstant(constantData);
  const auto vertexBuffer
    = graphicsMemory.Allocate(sizeof(Vertex) * vertices.size());
  memcpy(
    vertexBuffer.Memory(), vertices.data(), sizeof(Vertex) * vertices.size());

  const D3D12_VERTEX_BUFFER_VIEW vertexBufferView {
    .BufferLocation = vertexBuffer.GpuAddress(),
    .SizeInBytes = static_cast<UINT>(vertexBuffer.Size()),
    .StrideInBytes = sizeof(Vertex),
  };

  const auto commandList = mNextFrame->mCommandList;
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->SetGraphicsRootConstantBufferView(
    0, constantBuffer.GpuAddress());
  const auto heap = mShaderResourceViewHeap->Heap();
  commandList->SetDescriptorHeaps(1, &heap);
  commandList->SetGraphicsRootDescriptorTable(
    1,
    mShaderResourceViewHeap->GetGpuHandle(
      numeric_cast<std::size_t>(heapOffset)));
  commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
  commandList->DrawInstanced(vertices.size(), 1, 0, 0);
}

}// namespace OpenKneeboard::D3D12