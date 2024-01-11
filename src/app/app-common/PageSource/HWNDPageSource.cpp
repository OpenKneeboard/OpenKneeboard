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
#include <OpenKneeboard/HWNDPageSource.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/WindowCaptureControl.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/handles.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/weak_wrap.h>

#include <winrt/Microsoft.Graphics.Display.h>
#include <winrt/Microsoft.UI.Interop.h>
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

// TODO: error handling :)

namespace OpenKneeboard {

static std::unordered_map<HWND, HWNDPageSource*> gInstances;
static unique_hhook gHook;
static UINT gControlMessage;

std::shared_ptr<HWNDPageSource> HWNDPageSource::Create(
  const DXResources& dxr,
  KneeboardState* kneeboard,
  HWND window,
  const Options& options) noexcept {
  static_assert(with_final_release<HWNDPageSource>);
  if (!gControlMessage) {
    gControlMessage
      = RegisterWindowMessageW(WindowCaptureControl::WindowMessageName);
    if (!gControlMessage) {
      dprintf("Failed to Register a window message: {}", GetLastError());
    }
  }
  auto ret = shared_with_final_release(
    new HWNDPageSource(dxr, kneeboard, window, options));
  if (!ret->HaveWindow()) {
    return nullptr;
  }
  ret->Init();
  return ret;
}

winrt::fire_and_forget HWNDPageSource::Init() noexcept {
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

  co_await wil::resume_foreground(mDQC.DispatcherQueue());
  {
    const std::unique_lock d2dLock(mDXR);

    auto wgiFactory = winrt::get_activation_factory<WGC::GraphicsCaptureItem>();
    auto interopFactory = wgiFactory.as<IGraphicsCaptureItemInterop>();
    WGC::GraphicsCaptureItem item {nullptr};
    try {
      winrt::check_hresult(interopFactory->CreateForWindow(
        mWindow,
        winrt::guid_of<WGC::GraphicsCaptureItem>(),
        winrt::put_abi(item)));
    } catch (const winrt::hresult_error& e) {
      dprintf(
        "Error initializing Windows::Graphics::Capture::GraphicsCaptureItem "
        "for window: "
        "{} ({})",
        winrt::to_string(e.message()),
        static_cast<int64_t>(e.code().value));

      // We can't capture full-screen windows; if that's the problem, capture
      // the full screen instead :)
      RECT windowRect;
      if (!GetWindowRect(mWindow, &windowRect)) {
        dprint("Failed to get window rect");
        co_return;
      }
      const auto monitor = MonitorFromWindow(mWindow, MONITOR_DEFAULTTONULL);
      MONITORINFOEXW monitorInfo {sizeof(MONITORINFOEXW)};
      if (!GetMonitorInfoW(monitor, &monitorInfo)) {
        dprint("Failed to get monitor info");
        co_return;
      }
      if (windowRect != monitorInfo.rcMonitor) {
        co_return;
      }

      try {
        winrt::check_hresult(interopFactory->CreateForMonitor(
          monitor,
          winrt::guid_of<WGC::GraphicsCaptureItem>(),
          winrt::put_abi(item)));
      } catch (const winrt::hresult_error& e) {
        dprintf(
          "Error initializing Windows::Graphics::Capture::GraphicsCaptureItem "
          "for monitor: "
          "{} ({})",
          winrt::to_string(e.message()),
          static_cast<int64_t>(e.code().value));
        co_return;
      }
    }

    winrt::com_ptr<IInspectable> inspectable {nullptr};
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(

      mDXR.mDXGIDevice.get(),
      reinterpret_cast<IInspectable**>(winrt::put_abi(mWinRTD3DDevice))));

    mFramePool = WGC::Direct3D11CaptureFramePool::Create(
      mWinRTD3DDevice,
      WGDX::DirectXPixelFormat::R16G16B16A16Float,
      2,
      item.Size());
    mFramePool.FrameArrived(
      [this](const auto&, const auto&) { this->OnFrame(); });

    winrt::com_ptr<ID2D1ColorContext1> srgb, scrgb;
    auto d2d5 = mDXR.mD2DDeviceContext.as<ID2D1DeviceContext5>();
    d2d5->CreateColorContextFromDxgiColorSpace(
      DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, srgb.put());
    d2d5->CreateColorContextFromDxgiColorSpace(
      DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709, scrgb.put());
    winrt::check_hresult(mDXR.mD2DDeviceContext->CreateEffect(
      CLSID_D2D1ColorManagement, mD2DColorManagement.put()));
    mD2DColorManagement->SetValue(
      D2D1_COLORMANAGEMENT_PROP_SOURCE_COLOR_CONTEXT, scrgb.get());
    mD2DColorManagement->SetValue(
      D2D1_COLORMANAGEMENT_PROP_DESTINATION_COLOR_CONTEXT, srgb.get());

    {
      winrt::check_hresult(mDXR.mD2DDeviceContext->CreateEffect(
        CLSID_D2D1WhiteLevelAdjustment, mD2DWhiteLevel.put()));
      mD2DWhiteLevel->SetValue(
        D2D1_WHITELEVELADJUSTMENT_PROP_INPUT_WHITE_LEVEL,
        D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL);

      // DisplayInformation::CreateForWindowId only works for windows owned by
      // this thread (and process)... so we'll jump through some hoops.
      const auto monitor = MonitorFromWindow(mWindow, MONITOR_DEFAULTTONULL);
      auto displayID = winrt::Microsoft::UI::GetDisplayIdFromMonitor(monitor);
      auto di = winrt::Microsoft::Graphics::Display::DisplayInformation::
        CreateForDisplayId(displayID);
      auto aci = di.GetAdvancedColorInfo();
      // WideColorGamut and HighDynamicRange are distinct options here, but
      // I think they can both be treated the same; for non-HDR displays,
      // ACI reports standard white level, and for everything except SDR
      // we *should* have the color luminance points
      const auto isSDR = aci.CurrentAdvancedColorKind()
        == winrt::Microsoft::Graphics::Display::DisplayAdvancedColorKind::
          StandardDynamicRange;

      mD2DWhiteLevel->SetValue(
        D2D1_WHITELEVELADJUSTMENT_PROP_OUTPUT_WHITE_LEVEL,
        isSDR ? D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL
              : static_cast<FLOAT>(aci.SdrWhiteLevelInNits()));
    }

    mD2DFirstEffect = mD2DWhiteLevel.get();
    mD2DColorManagement->SetInputEffect(0, mD2DWhiteLevel.get(), true);
    mD2DLastEffect = mD2DColorManagement.get();

    mCaptureSession = mFramePool.CreateCaptureSession(item);
    mCaptureSession.IsCursorCaptureEnabled(mOptions.mCaptureCursor);
    if (supportsBorderRemoval) {
      mCaptureSession.IsBorderRequired(false);
    }
    mCaptureSession.StartCapture();

    mCaptureItem = item;
    mCaptureItem.Closed([this](auto&, auto&) {
      this->mWindow = {};
      this->evWindowClosedEvent.Emit();
    });
  }

  co_await winrt::resume_after(std::chrono::milliseconds(100));

  PostMessageW(
    mWindow,
    gControlMessage,
    static_cast<WPARAM>(WindowCaptureControl::WParam::Initialize),
    reinterpret_cast<LPARAM>(mWindow));
}

HWNDPageSource::HWNDPageSource(
  const DXResources& dxr,
  KneeboardState* kneeboard,
  HWND window,
  const Options& options)
  : mDXR(dxr), mWindow(window), mOptions(options) {
  if (!window) {
    return;
  }
  if (!WGC::GraphicsCaptureSession::IsSupported()) {
    return;
  }

  mDQC = winrt::Windows::System::DispatcherQueueController::
    CreateOnDedicatedThread();
}

bool HWNDPageSource::HaveWindow() const {
  return static_cast<bool>(mWindow);
}

// Destruction is handled in final_release instead
HWNDPageSource::~HWNDPageSource() = default;

winrt::fire_and_forget HWNDPageSource::final_release(
  std::unique_ptr<HWNDPageSource> p) {
  for (auto& [hwnd, handles]: p->mHooks) {
    if (handles.mHook32Subprocess) {
      TerminateProcess(handles.mHook32Subprocess.get(), 0);
      handles.mHook32Subprocess = {};
    }
  }

  p->RemoveAllEventListeners();
  if (!p->mDQC) {
    co_await p->mUIThread;
    co_return;
  }

  // Switch to DQ thread to clean up the Windows.Graphics.Capture objects that
  // were created in that thread
  co_await winrt::resume_foreground(p->mDQC.DispatcherQueue());
  if (p->mFramePool) {
    p->mCaptureSession.Close();
    p->mFramePool.Close();
    p->mCaptureSession = {nullptr};
    p->mCaptureItem = {nullptr};
    p->mFramePool = {nullptr};
  }

  co_await p->mUIThread;
  co_await p->mDQC.ShutdownQueueAsync();
  p->mDQC = {nullptr};
  const std::unique_lock d2dLock(p->mDXR);
  p->mTexture = nullptr;
}

PageIndex HWNDPageSource::GetPageCount() const {
  if (this->HaveWindow()) {
    return 1;
  }
  return 0;
}

std::vector<PageID> HWNDPageSource::GetPageIDs() const {
  if (this->HaveWindow()) {
    return {mPageID};
  }
  return {};
}

ScalingKind HWNDPageSource::GetScalingKind(PageID) {
  if (mTexture) {
    return ScalingKind::Bitmap;
  }
  // We can render errors at any resolution
  return ScalingKind::Vector;
}

D2D1_SIZE_U HWNDPageSource::GetNativeContentSize(PageID) {
  if (!mTexture) {
    return {};
  }

  const auto box = this->GetContentBox();
  return {box.right - box.left, box.bottom - box.top};
}

D3D11_BOX HWNDPageSource::GetContentBox() const {
  if (mOptions.mCaptureArea != CaptureArea::ClientArea) {
    if (mOptions.mCaptureArea != CaptureArea::FullWindow) {
      dprint("Invalid capture area specified, defaulting to full window");
      OPENKNEEBOARD_BREAK;
    }
    return {0, 0, 0, mContentSize.width, mContentSize.height, 1};
  }

  // Client area requested
  const auto clientArea = this->GetClientArea();
  if (!clientArea) {
    return {0, 0, 0, mContentSize.width, mContentSize.height, 1};
  }

  return {
    static_cast<UINT>(clientArea->left),
    static_cast<UINT>(clientArea->top),
    0,
    static_cast<UINT>(clientArea->right),
    static_cast<UINT>(clientArea->bottom),
    1,
  };
}

void HWNDPageSource::RenderPage(
  RenderTarget* rt,
  PageID,
  const D2D1_RECT_F& rect) {
  if (!mTexture) {
    return;
  }
  if (!this->HaveWindow()) {
    return;
  }

  const auto contentBox = this->GetContentBox();
  const D2D1_RECT_F srcRect {
    static_cast<FLOAT>(contentBox.left),
    static_cast<FLOAT>(contentBox.top),
    static_cast<FLOAT>(contentBox.right),
    static_cast<FLOAT>(contentBox.bottom),
  };

  auto d2d = rt->d2d();
  mD2DFirstEffect->SetInput(0, mBitmap.get(), true);
  const auto scale = (rect.right - rect.left) / mContentSize.width;
  d2d->SetTransform(
    D2D1::Matrix3x2F::Scale({scale, scale})
    * D2D1::Matrix3x2F::Translation({rect.left, rect.top}));
  d2d->DrawImage(mD2DLastEffect, nullptr, &srcRect);

  mNeedsRepaint = false;
}

void HWNDPageSource::OnFrame() {
  EventDelay delay;
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "HWNDPageSource::OnFrame");
  scope_guard traceOnException([&activity]() {
    if (std::uncaught_exceptions() > 0) {
      TraceLoggingWriteStop(
        activity,
        "HWNDPageSource::OnFrame",
        TraceLoggingValue("UncaughtExceptions", "Result"));
    }
  });
  auto frame = mFramePool.TryGetNextFrame();
  if (!frame) {
    TraceLoggingWriteStop(
      activity, __FUNCTION__, TraceLoggingValue("NoFrame", "Result"));
    return;
  }
  TraceLoggingWriteTagged(activity, "HaveFrame");

  auto wgdxSurface = frame.Surface();
  auto interopSurface = wgdxSurface.as<
    ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
  winrt::com_ptr<IDXGISurface> nativeSurface;
  winrt::check_hresult(
    interopSurface->GetInterface(IID_PPV_ARGS(nativeSurface.put())));
  auto d3dSurface = nativeSurface.as<ID3D11Texture2D>();
  D3D11_TEXTURE2D_DESC surfaceDesc {};
  d3dSurface->GetDesc(&surfaceDesc);
  const auto contentSize = frame.ContentSize();

  TraceLoggingWriteTagged(activity, "WaitingForLock");
  const std::unique_lock d2dLock(mDXR);
  TraceLoggingWriteTagged(activity, "Locked");
  winrt::com_ptr<ID3D11DeviceContext> ctx;
  mDXR.mD3DDevice->GetImmediateContext(ctx.put());

  if (
    contentSize.Width > surfaceDesc.Width
    || contentSize.Height > surfaceDesc.Height) {
    auto size = contentSize;
    // Don't create massive buffers if it just moves between a few fixed
    // sizes, but create full-screen buffers for smoothness if it's being
    // resized a bunch
    if (++mRecreations > 10) {
      const auto monitor = MonitorFromWindow(mWindow, MONITOR_DEFAULTTONULL);
      if (monitor) {
        MONITORINFO info {sizeof(MONITORINFO)};
        if (GetMonitorInfoW(monitor, &info)) {
          const auto monitorWidth = info.rcMonitor.right - info.rcMonitor.left;
          const auto monitorHeight = info.rcMonitor.bottom - info.rcMonitor.top;
          dprintf(
            L"Window capture monitor is {}x{}", monitorWidth, monitorHeight);
          if (monitorWidth > size.Width) {
            size.Width = monitorWidth;
          }
          if (monitorHeight > size.Height) {
            size.Height = monitorHeight;
          }
        }
      }
    }
    TraceLoggingWriteTagged(
      activity,
      "RecreatingPool",
      TraceLoggingValue(size.Width, "Width"),
      TraceLoggingValue(size.Height, "Height"));
    mFramePool.Recreate(
      mWinRTD3DDevice, WGDX::DirectXPixelFormat::R16G16B16A16Float, 2, size);
    TraceLoggingWriteStop(
      activity,
      "HWNDPageSource::OnFrame",
      TraceLoggingValue("RecreatedFramePool", "Result"));
    return;
  }

  if (mTexture) {
    D3D11_TEXTURE2D_DESC desc {};
    mTexture->GetDesc(&desc);
    if (surfaceDesc.Width != desc.Width || surfaceDesc.Height != desc.Height) {
      TraceLoggingWriteTagged(activity, "ResettingTexture");
      mTexture = nullptr;
    }
  }

  if (!mTexture) {
    winrt::check_hresult(
      mDXR.mD3DDevice->CreateTexture2D(&surfaceDesc, nullptr, mTexture.put()));
    mBitmap = nullptr;
    winrt::check_hresult(mDXR.mD2DDeviceContext->CreateBitmapFromDxgiSurface(
      mTexture.as<IDXGISurface>().get(), nullptr, mBitmap.put()));
    TraceLoggingWriteTagged(activity, "CreatedTexture");
  }

  mContentSize = {
    static_cast<UINT>(contentSize.Width),
    static_cast<UINT>(contentSize.Height)};
  TraceLoggingWriteTagged(
    activity,
    "ContentSize",
    TraceLoggingValue(contentSize.Width, "Width"),
    TraceLoggingValue(contentSize.Height, "Height"));

  D3D11_BOX box {this->GetContentBox()};
  D3D11_TEXTURE2D_DESC desc {};
  mTexture->GetDesc(&desc);
  box.left = std::min(box.left, desc.Width);
  box.right = std::min(box.right, desc.Width);
  box.top = std::min(box.top, desc.Height);
  box.bottom = std::min(box.bottom, desc.Height);

  TraceLoggingWriteTagged(activity, "CopySubresourceRegion");
  ctx->CopySubresourceRegion(
    mTexture.get(), 0, 0, 0, 0, d3dSurface.get(), 0, &box);
  TraceLoggingWriteTagged(activity, "evNeedsRepaint");
  this->evNeedsRepaintEvent.Emit();
  TraceLoggingWriteStop(
    activity,
    "HWNDPageSource::OnFrame",
    TraceLoggingValue("Success", "Result"));
}

static std::tuple<HWND, POINT> RecursivelyResolveWindowAndPoint(
  HWND parent,
  const POINT& originalPoint) {
  // Adjust to client rect
  POINT clientScreenOrigin {0, 0};
  ClientToScreen(parent, &clientScreenOrigin);
  RECT windowRect {};
  GetWindowRect(parent, &windowRect);
  const auto clientLeft = clientScreenOrigin.x - windowRect.left;
  const auto clientTop = clientScreenOrigin.y - windowRect.top;

  POINT clientPoint {originalPoint.x - clientLeft, originalPoint.y - clientTop};
  ClientToScreen(parent, &clientPoint);
  const POINT screenPoint {clientPoint};

  while (true) {
    clientPoint = screenPoint;
    ScreenToClient(parent, &clientPoint);
    const auto child
      = ChildWindowFromPointEx(parent, clientPoint, CWP_SKIPTRANSPARENT);
    if (child == parent) {
      break;
    }
    parent = child;
  }
  return {parent, clientPoint};
}

void HWNDPageSource::PostCursorEvent(
  EventContext,
  const CursorEvent& ev,
  PageID) {
  if (!mTexture) {
    return;
  }

  // The event point should already be scaled to native content size
  const auto [target, point] = RecursivelyResolveWindowAndPoint(
    mWindow, {std::lround(ev.mX), std::lround(ev.mY)});
  if (!target) {
    return;
  }

  InstallWindowHooks(target);
  // In theory, we might be supposed to use ChildWindowFromPoint()
  // and MapWindowPoints() - on the other hand, doing that doesn't
  // seem to fix anything, but breaks Chrome

  WPARAM wParam {};
  if (ev.mButtons & 1) {
    wParam |= MK_LBUTTON;
  }
  if (ev.mButtons & (1 << 1)) {
    wParam |= MK_RBUTTON;
  }
  LPARAM lParam = MAKELPARAM(point.x, point.y);

  if (ev.mTouchState == CursorTouchState::NOT_NEAR_SURFACE) {
    if (mMouseButtons & 1) {
      PostMessage(target, WM_LBUTTONUP, wParam, lParam);
    }
    if (mMouseButtons & (1 << 1)) {
      PostMessage(target, WM_RBUTTONUP, wParam, lParam);
    }
    mMouseButtons = {};
    PostMessage(target, WM_MOUSELEAVE, 0, 0);
    return;
  }

  SendMessageW(
    target,
    gControlMessage,
    static_cast<WPARAM>(WindowCaptureControl::WParam::StartInjection),
    reinterpret_cast<LPARAM>(mWindow));
  const scope_guard unhook([=]() {
    SendMessageW(
      target,
      gControlMessage,
      static_cast<WPARAM>(WindowCaptureControl::WParam::EndInjection),
      0);
  });

  // We only pay attention to buttons (1) and (1 << 1)
  const auto buttons = ev.mButtons & 3;
  if (buttons == mMouseButtons) {
    SendMessageW(target, WM_MOUSEMOVE, wParam, lParam);
    return;
  }

  const auto down = (ev.mButtons & ~mMouseButtons);
  const auto up = (mMouseButtons & ~ev.mButtons);
  mMouseButtons = buttons;

  if (down & 1) {
    SendMessageW(target, WM_LBUTTONDOWN, wParam, lParam);
  }
  if (up & 1) {
    SendMessageW(target, WM_LBUTTONUP, wParam, lParam);
  }
  if (down & (1 << 1)) {
    SendMessageW(target, WM_RBUTTONDOWN, wParam, lParam);
  }
  if (up & (1 << 1)) {
    SendMessageW(target, WM_RBUTTONUP, wParam, lParam);
  }
}

bool HWNDPageSource::CanClearUserInput() const {
  return false;
}

bool HWNDPageSource::CanClearUserInput(PageID) const {
  return false;
}

void HWNDPageSource::ClearUserInput(PageID) {
  // nothing to do here
}

void HWNDPageSource::ClearUserInput() {
  // nothing to do here
}

std::optional<RECT> HWNDPageSource::GetClientArea() const {
  if (mOptions.mCaptureArea != CaptureArea::ClientArea) {
    dprintf("{} called, but capture area is not client area", __FUNCTION__);
    OPENKNEEBOARD_BREAK;
    return {};
  }

  if (IsIconic(mWindow)) {
    return {};
  }

  RECT clientRect;
  if (!GetClientRect(mWindow, &clientRect)) {
    return {};
  }

  // The capture includes the extended frame bounds, not just the standard
  // ones, so we need to fetch those instead of using `GetWindowRect()`
  RECT windowRect {};
  if (
    DwmGetWindowAttribute(
      mWindow, DWMWA_EXTENDED_FRAME_BOUNDS, &windowRect, sizeof(windowRect))
    != S_OK) {
    return {};
  }

  // Convert client coordinates to screen coordinates; needed as
  // `GetClientRect()` returns client coordinates, so the top left of the
  // client rect is always (0, 0), which isn't useful
  MapWindowPoints(
    mWindow, HWND_DESKTOP, reinterpret_cast<POINT*>(&clientRect), 2);

  const auto windowWidth = windowRect.right - windowRect.left;
  const auto scale = static_cast<float>(mContentSize.width) / windowWidth;

  return {RECT {
    std::lround((clientRect.left - windowRect.left) * scale),
    std::lround((clientRect.top - windowRect.top) * scale),
    std::lround((clientRect.right - clientRect.left) * scale),
    std::lround((clientRect.bottom - clientRect.top) * scale),
  }};
}

void HWNDPageSource::InstallWindowHooks(HWND target) {
  if (mHooks.contains(target)) {
    return;
  }

  BOOL is32Bit {FALSE};
  {
    DWORD processID {};
    GetWindowThreadProcessId(target, &processID);
    winrt::handle process {
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, processID)};
    if (!process) {
      return;
    }
    winrt::check_bool(IsWow64Process(process.get(), &is32Bit));
  }

  if (!is32Bit) {
    // Natively use SetWindowsHookEx()
    mHooks[target].mHooks64 = WindowCaptureControl::InstallHooks(target);
    return;
  }

  // We need a 32-bit subprocess to install our hook

  const auto helper = (Filesystem::GetRuntimeDirectory()
                       / RuntimeFiles::WINDOW_CAPTURE_HOOK_32BIT_HELPER)
                        .wstring();
  const auto commandLine = std::format(
    L"{} {}",
    reinterpret_cast<uint64_t>(target),
    static_cast<uint64_t>(GetCurrentProcessId()));

  SHELLEXECUTEINFOW shellExecuteInfo {
    .cbSize = sizeof(SHELLEXECUTEINFOW),
    .fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS,
    .lpVerb = L"open",
    .lpFile = helper.c_str(),
    .lpParameters = commandLine.c_str(),
  };
  winrt::check_bool(ShellExecuteExW(&shellExecuteInfo));

  mHooks[target].mHook32Subprocess.attach(shellExecuteInfo.hProcess);
}

}// namespace OpenKneeboard
