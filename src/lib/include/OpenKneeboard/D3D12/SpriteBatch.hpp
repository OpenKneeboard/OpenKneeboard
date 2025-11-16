// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/D3D.hpp>
#include <OpenKneeboard/Pixels.hpp>

#include <directxtk12/DescriptorHeap.h>
#include <OpenKneeboard/Shaders/SpriteBatch.hpp>

#include <shims/winrt/base.h>

#include <d3d12.h>

#include <span>

#include <DirectXColors.h>

namespace OpenKneeboard::D3D12 {

class SpriteBatch {
 public:
  SpriteBatch() = delete;
  SpriteBatch(const SpriteBatch&) = delete;
  SpriteBatch(SpriteBatch&&) = delete;
  SpriteBatch& operator=(const SpriteBatch&) = delete;
  SpriteBatch& operator=(SpriteBatch&&) = delete;

  SpriteBatch(ID3D12Device*, ID3D12CommandQueue*, DXGI_FORMAT);
  ~SpriteBatch();

  void Begin(
    ID3D12GraphicsCommandList*,
    const D3D12_CPU_DESCRIPTOR_HANDLE& renderTargetView,
    const PixelSize& rtvSize);
  void Clear(DirectX::XMVECTORF32 color = DirectX::Colors::Transparent);

  /**
   * @param sourceSize the entire size of the source resource
   * @param sourceRect the rectangle of the source resource to copy; if we're
   * not drawing the entire source resource, the size of this rect will be
   * smaller than `sourceSize`.
   */
  void Draw(
    ID3D12Resource* source,
    const PixelSize& sourceSize,
    const PixelRect& sourceRect,
    const PixelRect& destRect,
    const DirectX::XMVECTORF32 tint = DirectX::Colors::White);
  void End();

 private:
  using Vertex = Shaders::SpriteBatch::Vertex;
  // "Uniform" is Vulkan terminology, "CBuffer" for D3D/HLSL
  using CBuffer = Shaders::SpriteBatch::UniformBuffer;
  static constexpr auto MaxInflightFrames = 2;

  static constexpr auto MaxSpritesPerBatch
    = Shaders::SpriteBatch::MaxSpritesPerBatch;
  static constexpr auto VerticesPerSprite
    = Shaders::SpriteBatch::VerticesPerSprite;
  static constexpr auto MaxVerticesPerBatch
    = Shaders::SpriteBatch::MaxVerticesPerBatch;

  ID3D12Device* mDevice {nullptr};
  ID3D12CommandQueue* mCommandQueue {nullptr};

  winrt::com_ptr<ID3D12RootSignature> mRootSignature;
  winrt::com_ptr<ID3D12PipelineState> mGraphicsPipeline;
  std::unique_ptr<DirectX::DescriptorHeap> mShaderResourceViewHeap;

  // Use modulo MaxInflightFrames for heap offset
  uint64_t mDrawCount {};

  struct Sprite {
    ID3D12Resource* mSource { nullptr };
    PixelSize mSourceSize;
    PixelRect mSourceRect;
    PixelRect mDestRect;
    DirectX::XMVECTORF32 mTint {};
  };

  struct NextFrame {
    ID3D12GraphicsCommandList* mCommandList {nullptr};
    D3D12_CPU_DESCRIPTOR_HANDLE mRenderTargetView {};
    PixelSize mRenderTargetViewSize {};

    std::vector<Sprite> mSprites;
  };

  std::optional<NextFrame> mNextFrame;
};

}// namespace OpenKneeboard::D3D12