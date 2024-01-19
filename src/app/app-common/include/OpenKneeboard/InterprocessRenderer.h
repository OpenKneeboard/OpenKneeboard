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
#pragma once

#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/IKneeboardView.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/SHM.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/final_release_deleter.h>

#include <shims/winrt/base.h>

#include <memory>
#include <mutex>
#include <optional>

#include <d2d1.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <d3d11_3.h>

namespace OpenKneeboard {
class CursorEvent;
class CursorRenderer;
class ITab;
class ToolbarAction;
struct GameInstance;
struct DXResources;

class InterprocessRenderer final
  : private EventReceiver,
    public std::enable_shared_from_this<InterprocessRenderer> {
 public:
  ~InterprocessRenderer();
  static winrt::fire_and_forget final_release(
    std::unique_ptr<InterprocessRenderer>);

  static std::shared_ptr<InterprocessRenderer> Create(
    const DXResources&,
    KneeboardState*);

 private:
  InterprocessRenderer();
  // Split out from the constructor so that `shared_from_this()` is valid
  // when invoked
  void Initialize(const DXResources&, KneeboardState*);

  // If we replace the shared_ptr while a draw is in progress,
  // we need to delay things a little
  static std::mutex sSingleInstance;
  std::unique_lock<std::mutex> mInstanceLock;
  winrt::apartment_context mOwnerThread;

  EventContext mEventContext;
  OpenKneeboard::SHM::Writer mSHM;
  DXResources mDXR;

  KneeboardState* mKneeboard = nullptr;

  bool mNeedsRepaint = true;

  std::atomic_flag mRendering;

  struct IPCTextureResources {
    winrt::com_ptr<ID3D11Texture2D> mTexture;
    winrt::com_ptr<ID3D11RenderTargetView> mRenderTargetView;

    winrt::handle mTextureHandle;
    PixelSize mTextureSize;

    winrt::com_ptr<ID3D11Fence> mFence;
    winrt::handle mFenceHandle;
    bool mNewFence {true};

    D3D11_VIEWPORT mViewport {};
  };

  std::array<IPCTextureResources, TextureCount> mIPCSwapchain;

  IPCTextureResources* GetIPCTextureResources(
    uint8_t textureIndex,
    const PixelSize&);

  std::shared_ptr<RenderTarget> mCanvas;
  PixelSize mCanvasSize;

  void InitializeCanvas(const PixelSize&);

  std::shared_ptr<GameInstance> mCurrentGame;

  void MarkDirty();
  void RenderNow() noexcept;
  SHM::LayerConfig RenderLayer(
    const ViewRenderInfo&,
    const PixelRect& bounds) noexcept;

  void SubmitFrame(
    const std::vector<SHM::LayerConfig>&,
    uint64_t inputLayerID) noexcept;

  void OnGameChanged(DWORD processID, const std::shared_ptr<GameInstance>&);
};

}// namespace OpenKneeboard
