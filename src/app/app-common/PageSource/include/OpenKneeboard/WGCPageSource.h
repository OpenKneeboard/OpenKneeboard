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
#include <OpenKneeboard/IPageSource.h>
#include <OpenKneeboard/IPageSourceWithInternalCaching.h>

#include <OpenKneeboard/audited_ptr.h>
#include <OpenKneeboard/handles.h>

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
class WGCPageSource : public virtual IPageSource,
                      public virtual IPageSourceWithInternalCaching,
                      public virtual EventReceiver,
                      public std::enable_shared_from_this<WGCPageSource> {
 public:
  virtual ~WGCPageSource();
  static winrt::fire_and_forget final_release(std::unique_ptr<WGCPageSource>);

  virtual PageIndex GetPageCount() const final override;
  virtual std::vector<PageID> GetPageIDs() const final override;
  virtual PreferredSize GetPreferredSize(PageID) final override;

  virtual void RenderPage(RenderTarget*, PageID, const D2D1_RECT_F& rect)
    final override;

  struct Options {
    bool mCaptureCursor {false};

    constexpr auto operator<=>(const Options&) const noexcept = default;
  };

 protected:
  WGCPageSource(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const Options& options);
  winrt::fire_and_forget Init() noexcept;

  virtual winrt::Windows::Foundation::IAsyncAction InitializeInCaptureThread()
    = 0;
  virtual std::optional<float> GetHDRWhiteLevelInNits() const = 0;
  virtual winrt::Windows::Graphics::DirectX::DirectXPixelFormat GetPixelFormat()
    const
    = 0;
  virtual winrt::Windows::Graphics::Capture::GraphicsCaptureItem
  CreateWGCaptureItem()
    = 0;
  virtual PixelRect GetContentRect(const PixelSize& captureSize) = 0;
  virtual PixelSize GetSwapchainDimensions(const PixelSize& captureSize) = 0;

  winrt::fire_and_forget ForceResize(const PixelSize&);

 private:
  WGCPageSource() = delete;
  void OnFrame();

  Options mOptions;

  winrt::apartment_context mUIThread;
  audited_ptr<DXResources> mDXR;
  PageID mPageID {};

  PixelSize mSwapchainDimensions;

  winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice
    mWinRTD3DDevice {nullptr};
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool mFramePool {
    nullptr};
  winrt::Windows::Graphics::Capture::GraphicsCaptureSession mCaptureSession {
    nullptr};
  winrt::Windows::Graphics::Capture::GraphicsCaptureItem mCaptureItem {nullptr};

  winrt::Windows::System::DispatcherQueueController mDQC {nullptr};

  PixelSize mCaptureSize {};
  winrt::com_ptr<ID3D11Texture2D> mTexture;
  winrt::com_ptr<ID3D11ShaderResourceView> mShaderResourceView;
  bool mNeedsRepaint {true};
};

}// namespace OpenKneeboard
