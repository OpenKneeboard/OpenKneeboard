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
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/WGCRenderer.h>
#include <OpenKneeboard/WindowCaptureControl.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/handles.h>
#include <OpenKneeboard/scope_exit.h>

#include <shims/winrt/Microsoft.UI.Interop.h>

#include <winrt/Microsoft.Graphics.Display.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>

#include <wil/cppwinrt.h>
#include <wil/cppwinrt_helpers.h>

#include <Windows.Graphics.Capture.Interop.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>

#include <mutex>

#include <dwmapi.h>
#include <libloaderapi.h>
#include <shellapi.h>
#include <wow64apiset.h>

namespace WGC = winrt::Windows::Graphics::Capture;
namespace WGDX = winrt::Windows::Graphics::DirectX;

namespace OpenKneeboard {

winrt::fire_and_forget WGCRenderer::Init() noexcept {
  const auto keepAlive = shared_from_this();

  // Requires Windows 11
  bool supportsBorderRemoval = false;
  try {
    supportsBorderRemoval
      = winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
        L"Windows.Graphics.Capture.GraphicsCaptureSession",
        L"IsBorderRequired");
    if (supportsBorderRemoval) {
      co_await WGC::GraphicsCaptureAccess::RequestAccessAsync(
        WGC::GraphicsCaptureAccessKind::Borderless);
    }

  } catch (const winrt::hresult_class_not_registered&) {
    supportsBorderRemoval = false;
  }

  // Would switch threads here, but now we're using the UI thread for everything
  // :)
  {
    co_await this->InitializeContentToCapture();
    co_await mUIThread;
    const std::unique_lock d2dlock(*mDXR);

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item {nullptr};
    try {
      item = this->CreateWGCaptureItem();
    } catch (const winrt::hresult_error& e) {
      OPENKNEEBOARD_BREAK;
      co_return;
    }
    if (!item) {
      dprint("Failed to create Windows::Graphics::CaptureItem");
      co_return;
    }

    const auto size = item.Size();
    if (size.Width < 1 || size.Height < 1) {
      OPENKNEEBOARD_BREAK;
      co_return;
    }

    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(
      mDXR->mDXGIDevice.get(),
      reinterpret_cast<IInspectable**>(winrt::put_abi(mWinRTD3DDevice))));

    // WGC does not support direct capture of sRGB
    mFramePool = WGC::Direct3D11CaptureFramePool::CreateFreeThreaded(
      mWinRTD3DDevice,
      this->GetPixelFormat(),
      WGCRenderer::SwapchainLength,
      size);

    mCaptureSession = mFramePool.CreateCaptureSession(item);
    mCaptureSession.IsCursorCaptureEnabled(mOptions.mCaptureCursor);
    if (supportsBorderRemoval) {
      mCaptureSession.IsBorderRequired(false);
    }
    mCaptureSession.StartCapture();

    mCaptureItem = item;
  }

  co_await mUIThread;
}

WGCRenderer::WGCRenderer(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard,
  const Options& options)
  : mDXR(dxr), mOptions(options) {
  if (!WGC::GraphicsCaptureSession::IsSupported()) {
    return;
  }

  AddEventListener(
    kneeboard->evFrameTimerPreEvent, [this]() { this->PreOKBFrame(); });
}

// Destruction is handled in final_release instead
WGCRenderer::~WGCRenderer() = default;

winrt::fire_and_forget WGCRenderer::final_release(
  std::unique_ptr<WGCRenderer> p) {
  p->RemoveAllEventListeners();
  co_await p->mUIThread;

  if (p->mCaptureSession) {
    p->mCaptureSession.Close();
    // Closing queues up some events in the main loop without keeping a strong
    // reference :( Give them some time to clean up
    co_await winrt::resume_after(std::chrono::milliseconds(100));
    co_await p->mUIThread;
  }

  p->mNextFrame = {nullptr};
  p->mCaptureSession = {nullptr};
  p->mCaptureItem = {nullptr};
  p->mFramePool = {nullptr};

  p->mTexture = nullptr;
}

bool WGCRenderer::HaveCaptureItem() const {
  return !!mCaptureItem;
}

PreferredSize WGCRenderer::GetPreferredSize() const {
  if (!mTexture) {
    return {};
  }

  return {
    this->GetContentRect(mCaptureSize).mSize,
    ScalingKind::Bitmap,
  };
}

void WGCRenderer::Render(RenderTarget* rt, const PixelRect& rect) {
  if (!mCaptureItem) {
    return;
  }

  auto d3d = rt->d3d();
  if (mNextFrame) {
    this->OnWGCFrame();
  }
  if (!mTexture) {
    return;
  }

  auto color = DirectX::Colors::White;

  const auto whiteLevel = this->GetHDRWhiteLevelInNits();
  if (whiteLevel) {
    const auto dimming = D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL / (*whiteLevel);
    color = {dimming, dimming, dimming, 1};
  }

  auto sb = mDXR->mSpriteBatch.get();
  sb->Begin(d3d.rtv(), rt->GetDimensions());
  const auto sourceRect = this->GetContentRect(mCaptureSize);
  sb->Draw(mShaderResourceView.get(), sourceRect, rect, color);
  sb->End();

  mNeedsRepaint = false;
}

void WGCRenderer::OnWGCFrame() {
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "WGCRenderer::OnWGCFrame()");

  const auto keepAlive = shared_from_this();

  auto frame = mNextFrame;
  mNextFrame = {nullptr};
  if (!frame) {
    activity.StopWithResult("No Frame");
    return;
  }
  TraceLoggingWriteTagged(activity, "WGCRenderer::OnWGCFrame()/HaveFrame");

  auto wgdxSurface = frame.Surface();
  auto interopSurface = wgdxSurface.as<
    ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
  winrt::com_ptr<IDXGISurface> nativeSurface;
  winrt::check_hresult(
    interopSurface->GetInterface(IID_PPV_ARGS(nativeSurface.put())));
  auto d3dSurface = nativeSurface.as<ID3D11Texture2D>();
  D3D11_TEXTURE2D_DESC surfaceDesc {};
  d3dSurface->GetDesc(&surfaceDesc);
  const auto captureSize = frame.ContentSize();

  const auto swapchainDimensions
    = ((captureSize.Width <= mSwapchainDimensions.mWidth)
       && (captureSize.Height <= mSwapchainDimensions.mHeight))
    ? mSwapchainDimensions
    : this->GetSwapchainDimensions({
        static_cast<uint32_t>(captureSize.Width),
        static_cast<uint32_t>(captureSize.Height),
      });

  if (swapchainDimensions != mSwapchainDimensions) {
    OPENKNEEBOARD_TraceLoggingScope(
      "WGCRenderer::OnWGCFrame()/RecreatePool",
      TraceLoggingValue(swapchainDimensions.mWidth, "Width"),
      TraceLoggingValue(swapchainDimensions.mHeight, "Height"));
    std::unique_lock lock(*mDXR);
    mSwapchainDimensions = swapchainDimensions;
    mFramePool.Recreate(
      mWinRTD3DDevice,
      this->GetPixelFormat(),
      2,
      swapchainDimensions
        .StaticCast<int32_t, winrt::Windows::Graphics::SizeInt32>());
    return;
  }

  if (
    surfaceDesc.Width < captureSize.Width
    || surfaceDesc.Height < captureSize.Height) {
    return;
  }

  if (mTexture) {
    D3D11_TEXTURE2D_DESC desc {};
    mTexture->GetDesc(&desc);
    if (surfaceDesc.Width != desc.Width || surfaceDesc.Height != desc.Height) {
      TraceLoggingWriteTagged(
        activity, "WGCRenderer::OnWGCFrame()/ResettingTexture");
      mTexture = nullptr;
    }
  }

  if (!mTexture) {
    OPENKNEEBOARD_TraceLoggingScope("WGCRenderer::OnWGCFrame()/CreateTexture");
    std::unique_lock lock(*mDXR);
    auto desc = surfaceDesc;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = 0;

    winrt::check_hresult(
      mDXR->mD3D11Device->CreateTexture2D(&desc, nullptr, mTexture.put()));
    mShaderResourceView = nullptr;
    winrt::check_hresult(mDXR->mD3D11Device->CreateShaderResourceView(
      mTexture.get(), nullptr, mShaderResourceView.put()));
  }

  mCaptureSize = {
    static_cast<uint32_t>(captureSize.Width),
    static_cast<uint32_t>(captureSize.Height),
  };

  TraceLoggingWriteTagged(
    activity,
    "WGCRenderer::OnFrame()/CaptureSize",
    TraceLoggingValue(captureSize.Width, "Width"),
    TraceLoggingValue(captureSize.Height, "Height"));

  const auto contentRect = this->GetContentRect(mCaptureSize);

  const D3D11_BOX box {
    .left = static_cast<UINT>(
      std::min(contentRect.Left(), mSwapchainDimensions.Width())),
    .top = static_cast<UINT>(
      std::min(contentRect.Top(), mSwapchainDimensions.Height())),
    .front = 0,
    .right = static_cast<UINT>(
      std::min(contentRect.Right(), mSwapchainDimensions.Width())),
    .bottom = static_cast<UINT>(
      std::min(contentRect.Bottom(), mSwapchainDimensions.Height())),
    .back = 1,
  };
  mDXR->mD3D11ImmediateContext->CopySubresourceRegion(
    mTexture.get(), 0, 0, 0, 0, d3dSurface.get(), 0, &box);
  activity.Stop();
  {
    OPENKNEEBOARD_TraceLoggingScope(
      "WGCRenderer::OnWGCFrame()/CallingPostFrame");
    this->PostFrame();
  }
}

void WGCRenderer::PreOKBFrame() {
  if (!mFramePool) {
    return;
  }
  mNextFrame = mFramePool.TryGetNextFrame();
  if (mNextFrame) {
    evNeedsRepaintEvent.Emit();
  }
}

void WGCRenderer::PostFrame() {
}

winrt::fire_and_forget WGCRenderer::ForceResize(PixelSize size) {
  OPENKNEEBOARD_TraceLoggingScope(
    "WGCRenderer::ForceResize()",
    TraceLoggingValue(size.mWidth, "Width"),
    TraceLoggingValue(size.mHeight, "Height"));
  mThreadGuard.CheckThread();
  mFramePool.Recreate(
    mWinRTD3DDevice,
    this->GetPixelFormat(),
    WGCRenderer::SwapchainLength,
    size.StaticCast<int32_t, winrt::Windows::Graphics::SizeInt32>());
  co_return;
}

}// namespace OpenKneeboard
