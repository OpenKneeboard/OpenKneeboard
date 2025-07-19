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

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/IHasDisposeAsync.hpp>
#include <OpenKneeboard/PreferredSize.hpp>
#include <OpenKneeboard/ProcessShutdownBlock.hpp>
#include <OpenKneeboard/RenderTarget.hpp>
#include <OpenKneeboard/ThreadGuard.hpp>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/enable_shared_from_this.hpp>
#include <OpenKneeboard/handles.hpp>
#include <OpenKneeboard/task.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.System.h>

#include <memory>

namespace OpenKneeboard {

class KneeboardState;

namespace D3D11 {
class SpriteBatch;
}

// A page source using Windows::Graphics::Capture
class WGCRenderer : public virtual EventReceiver,
                    public IHasDisposeAsync,
                    public enable_shared_from_this<WGCRenderer> {
 public:
  virtual ~WGCRenderer();
  virtual task<void> DisposeAsync() noexcept override;

  std::optional<PreferredSize> GetPreferredSize() const;

  bool HaveCaptureItem() const;
  void Render(RenderTarget*, const PixelRect& rect);

  struct Options {
    bool mCaptureCursor {false};

    constexpr bool operator==(const Options&) const noexcept = default;
  };

  Event<> evNeedsRepaintEvent;
  Event<> evContentChangedEvent;

  OpenKneeboard::fire_and_forget ForceResize(PixelSize);

 protected:
  WGCRenderer(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const Options& options);
  [[nodiscard]]
  task<void> Init();

  virtual std::optional<float> GetHDRWhiteLevelInNits() const = 0;
  virtual winrt::Windows::Graphics::DirectX::DirectXPixelFormat GetPixelFormat()
    const
    = 0;
  virtual winrt::Windows::Graphics::Capture::GraphicsCaptureItem
  CreateWGCaptureItem()
    = 0;
  virtual PixelRect GetContentRect(const PixelSize& captureSize) const = 0;
  virtual PixelSize GetSwapchainDimensions(const PixelSize& captureSize) const
    = 0;

  ThreadGuard mThreadGuard;

 private:
  ProcessShutdownBlock mBlockShutdownUntilDestroyed;
  static constexpr int32_t SwapchainLength = 3;
  WGCRenderer() = delete;

  void PreOKBFrame();
  void OnWGCFrame(winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame);

  DisposalState mDisposal;
  winrt::apartment_context mUIThread;
  audited_ptr<DXResources> mDXR;
  Options mOptions;

  PageID mPageID {};

  PixelSize mSwapchainDimensions;

  winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice
    mWinRTD3DDevice {nullptr};
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool mFramePool {
    nullptr};
  winrt::Windows::Graphics::Capture::GraphicsCaptureSession mCaptureSession {
    nullptr};
  winrt::Windows::Graphics::Capture::GraphicsCaptureItem mCaptureItem {nullptr};

  PixelSize mCaptureSize {};
  winrt::com_ptr<ID3D11Texture2D> mTexture;
  winrt::com_ptr<ID3D11ShaderResourceView> mShaderResourceView;
};

}// namespace OpenKneeboard
