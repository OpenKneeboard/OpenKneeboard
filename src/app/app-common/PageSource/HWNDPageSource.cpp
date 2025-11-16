// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/D3D11.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/HWNDPageSource.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>
#include <OpenKneeboard/WindowCaptureControl.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/handles.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <shims/winrt/Microsoft.UI.Interop.h>

#include <libloaderapi.h>
#include <shellapi.h>

#include <winrt/Microsoft.Graphics.Display.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>

#include <wil/cppwinrt.h>

#include <Windows.Graphics.Capture.Interop.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>

#include <mutex>

#include <dwmapi.h>
#include <wow64apiset.h>

#include <wil/cppwinrt_helpers.h>

namespace WGC = winrt::Windows::Graphics::Capture;
namespace WGDX = winrt::Windows::Graphics::DirectX;

// TODO: error handling :)

namespace OpenKneeboard {

static UINT gControlMessage;

task<std::shared_ptr<HWNDPageSource>> HWNDPageSource::Create(
  audited_ptr<DXResources> dxr,
  KneeboardState* kneeboard,
  HWND window,
  Options options) noexcept {
  if (!gControlMessage) {
    gControlMessage
      = RegisterWindowMessageW(WindowCaptureControl::WindowMessageName);
    if (!gControlMessage) {
      dprint("Failed to Register a window message: {}", GetLastError());
    }
  }
  std::shared_ptr<HWNDPageSource> ret {
    new HWNDPageSource(dxr, kneeboard, window, options)};
  if (!ret->HaveWindow()) {
    co_return nullptr;
  }
  co_await ret->Init();
  co_return ret;
}

std::optional<float> HWNDPageSource::GetHDRWhiteLevelInNits() const {
  if (mIsHDR) {
    return mSDRWhiteLevelInNits;
  }
  return {};
}

winrt::Windows::Graphics::DirectX::DirectXPixelFormat
HWNDPageSource::GetPixelFormat() const {
  return mPixelFormat;
}

task<void> HWNDPageSource::Init() noexcept {
  co_await WGCRenderer::Init();
  co_return;
}

void HWNDPageSource::LogAdapter(HMONITOR monitor) {
  int adapterIndex = 0;
  winrt::com_ptr<IDXGIAdapter> adapter;
  while (mDXR->mDXGIFactory->EnumAdapters(adapterIndex++, adapter.put())
         == S_OK) {
    int outputIndex = 0;
    winrt::com_ptr<IDXGIOutput> output;
    while (adapter->EnumOutputs(outputIndex++, output.put()) == S_OK) {
      DXGI_OUTPUT_DESC outputDesc;
      output->GetDesc(&outputDesc);
      if (monitor == outputDesc.Monitor) {
        DXGI_ADAPTER_DESC adapterDesc;
        adapter->GetDesc(&adapterDesc);

        dprint(
          L"Capturing on monitor '{}' connected to adapter '{}' (LUID {:#x})",
          std::wstring_view {outputDesc.DeviceName},
          std::wstring_view {adapterDesc.Description},
          std::bit_cast<uint64_t>(adapterDesc.AdapterLuid));

        if (
          std::bit_cast<uint64_t>(adapterDesc.AdapterLuid)
          != mDXR->mAdapterLUID) {
          dprint.Warning(
            "Capture adapter LUID {:#x} != OKB adapter LUID {:#x}",
            std::bit_cast<uint64_t>(adapterDesc.AdapterLuid),
            mDXR->mAdapterLUID);
        }
        return;
      }
      output = nullptr;
    }
    adapter = nullptr;
  }
}

void HWNDPageSource::LogAdapter(HWND window) {
  auto monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
  LogAdapter(monitor);
}

winrt::Windows::Graphics::Capture::GraphicsCaptureItem
HWNDPageSource::CreateWGCaptureItem() {
  WGC::GraphicsCaptureItem item {nullptr};
  auto wgiFactory = winrt::get_activation_factory<WGC::GraphicsCaptureItem>();
  auto interopFactory = wgiFactory.as<IGraphicsCaptureItemInterop>();
  try {
    winrt::check_hresult(interopFactory->CreateForWindow(
      mCaptureWindow,
      winrt::guid_of<WGC::GraphicsCaptureItem>(),
      winrt::put_abi(item)));
    LogAdapter(mCaptureWindow);
  } catch (const winrt::hresult_error& e) {
    dprint(
      "Error initializing Windows::Graphics::Capture::GraphicsCaptureItem "
      "for window: "
      "{} ({})",
      winrt::to_string(e.message()),
      static_cast<int64_t>(e.code().value));

    // We can't capture full-screen windows; if that's the problem, capture
    // the full screen instead :)
    RECT windowRect;
    if (!GetWindowRect(mCaptureWindow, &windowRect)) {
      dprint("Failed to get window rect");
      return nullptr;
    }
    const auto monitor
      = MonitorFromWindow(mCaptureWindow, MONITOR_DEFAULTTONULL);
    MONITORINFOEXW monitorInfo {sizeof(MONITORINFOEXW)};
    if (!GetMonitorInfoW(monitor, &monitorInfo)) {
      dprint("Failed to get monitor info");
      return nullptr;
    }
    if (windowRect != monitorInfo.rcMonitor) {
      return nullptr;
    }

    try {
      winrt::check_hresult(interopFactory->CreateForMonitor(
        monitor,
        winrt::guid_of<WGC::GraphicsCaptureItem>(),
        winrt::put_abi(item)));
      LogAdapter(monitor);
    } catch (const winrt::hresult_error& e) {
      dprint(
        "Error initializing Windows::Graphics::Capture::GraphicsCaptureItem "
        "for monitor: "
        "{} ({})",
        winrt::to_string(e.message()),
        static_cast<int64_t>(e.code().value));
      return nullptr;
    }
  }

  {
    // DisplayInformation::CreateForWindowId only works for windows owned by
    // this thread (and process)... so we'll jump through some hoops.
    const auto monitor
      = MonitorFromWindow(mCaptureWindow, MONITOR_DEFAULTTONULL);
    auto displayID = winrt::Microsoft::UI::GetDisplayIdFromMonitor(monitor);
    auto di = winrt::Microsoft::Graphics::Display::DisplayInformation::
      CreateForDisplayId(displayID);
    auto aci = di.GetAdvancedColorInfo();
    // WideColorGamut and HighDynamicRange are distinct options here, but
    // I think they can both be treated the same; for non-HDR displays,
    // ACI reports standard white level, and for everything except SDR
    // we *should* have the color luminance points
    mIsHDR = aci.CurrentAdvancedColorKind()
      != winrt::Microsoft::Graphics::Display::DisplayAdvancedColorKind::
        StandardDynamicRange;

    if (mIsHDR) {
      mPixelFormat = WGDX::DirectXPixelFormat::R16G16B16A16Float;
      mSDRWhiteLevelInNits = static_cast<FLOAT>(aci.SdrWhiteLevelInNits());
    } else {
      mPixelFormat = WGDX::DirectXPixelFormat::B8G8R8A8UIntNormalized;
      mSDRWhiteLevelInNits = D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL;
    }
  }

  item.Closed([this](auto&, auto&) {
    this->mCaptureWindow = {};
    this->mInputWindow = {};
    this->evWindowClosedEvent.Emit();
  });
  this->InitializeInputHook();
  return item;
}

OpenKneeboard::fire_and_forget HWNDPageSource::InitializeInputHook() noexcept {
  auto weak = weak_from_this();
  co_await winrt::resume_after(std::chrono::milliseconds(100));
  auto self = weak.lock();
  if (!self) {
    co_return;
  }
  PostMessageW(
    mCaptureWindow,
    gControlMessage,
    static_cast<WPARAM>(WindowCaptureControl::WParam::Initialize),
    reinterpret_cast<LPARAM>(mCaptureWindow));
}

HWNDPageSource::HWNDPageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard,
  HWND window,
  const Options& options)
  : WGCRenderer(dxr, kneeboard, options),
    mDXR(dxr),
    mCaptureWindow(window),
    mOptions(options) {
  this->AddEventListener(
    WGCRenderer::evNeedsRepaintEvent, IPageSource::evNeedsRepaintEvent);
  // Handle UWP input
  if (
    HWND child
    = FindWindowExW(window, {}, L"ApplicationFrameInputSinkWindow", nullptr)) {
    mInputWindow = child;
  } else {
    mInputWindow = window;
  }
}

bool HWNDPageSource::HaveWindow() const {
  return static_cast<bool>(mCaptureWindow);
}

HWNDPageSource::~HWNDPageSource() = default;

task<void> HWNDPageSource::DisposeAsync() noexcept {
  IPageSource::mThreadGuard.CheckThread();

  const auto disposing = co_await mDisposal.StartOnce();
  if (!disposing) {
    co_return;
  }

  auto self = shared_from_this();
  auto thread = mUIThread;

  this->RemoveAllEventListeners();
  for (auto& [hwnd, handles]: mHooks) {
    if (handles.mHook32Subprocess) {
      TerminateProcess(handles.mHook32Subprocess.get(), 0);
      handles.mHook32Subprocess = {};
    }
  }

  co_await WGCRenderer::DisposeAsync();
  co_await thread;
}

PixelRect HWNDPageSource::GetContentRect(const PixelSize& captureSize) const {
  if (mOptions.mCaptureArea != CaptureArea::ClientArea) {
    if (mOptions.mCaptureArea != CaptureArea::FullWindow) {
      dprint("Invalid capture area specified, defaulting to full window");
      OPENKNEEBOARD_BREAK;
    }
    return {{0, 0}, captureSize};
  }

  // Client area requested
  const auto clientArea = this->GetClientArea(captureSize);
  if (!clientArea) {
    return {{0, 0}, captureSize};
  }

  return *clientArea;
}

PixelSize HWNDPageSource::GetSwapchainDimensions(
  const PixelSize& contentSize) const {
  // Don't create massive buffers if it just moves between a few fixed
  // sizes, but create full-screen buffers for smoothness if it's being
  // resized a bunch
  if (++mRecreations <= 10) {
    return contentSize;
  }
  const auto monitor = MonitorFromWindow(mCaptureWindow, MONITOR_DEFAULTTONULL);
  if (!monitor) {
    return contentSize;
  }

  MONITORINFO info {sizeof(MONITORINFO)};
  if (!GetMonitorInfoW(monitor, &info)) {
    return contentSize;
  }
  const auto monitorWidth
    = static_cast<uint32_t>(info.rcMonitor.right - info.rcMonitor.left);
  const auto monitorHeight
    = static_cast<uint32_t>(info.rcMonitor.bottom - info.rcMonitor.top);
  dprint(L"Window capture monitor is {}x{}", monitorWidth, monitorHeight);
  return {
    std::max(contentSize.mWidth, monitorWidth),
    std::max(contentSize.mHeight, monitorHeight),
  };
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
  KneeboardViewID,
  const CursorEvent& ev,
  PageID) {
  if (!mCaptureWindow) {
    return;
  }

  // The event point should already be scaled to native content size
  const auto [target, point] = RecursivelyResolveWindowAndPoint(
    mInputWindow, {std::lround(ev.mX), std::lround(ev.mY)});
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

  if (ev.mTouchState == CursorTouchState::NotNearSurface) {
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
    reinterpret_cast<LPARAM>(mCaptureWindow));
  const scope_exit unhook([=]() {
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

std::optional<PixelRect> HWNDPageSource::GetClientArea(
  const PixelSize& captureSize) const {
  if (mOptions.mCaptureArea != CaptureArea::ClientArea) {
    dprint("{} called, but capture area is not client area", __FUNCTION__);
    OPENKNEEBOARD_BREAK;
    return {};
  }

  if (IsIconic(mCaptureWindow)) {
    return {};
  }

  RECT clientRect;
  if (!GetClientRect(mCaptureWindow, &clientRect)) {
    return {};
  }
  RECT windowRect;
  if (
    DwmGetWindowAttribute(
      mCaptureWindow,
      DWMWA_EXTENDED_FRAME_BOUNDS,
      &windowRect,
      sizeof(windowRect))
    != S_OK) {
    return {};
  }
  MapWindowPoints(
    HWND_DESKTOP, mCaptureWindow, reinterpret_cast<POINT*>(&windowRect), 2);

  const auto windowWidth = windowRect.right - windowRect.left;
  const auto scale = captureSize.Width<float>() / windowWidth;

  return Geometry2D::Rect<float> {
    {
      (clientRect.left - windowRect.left) * scale,
      (clientRect.top - windowRect.top) * scale,
    },
    {
      (clientRect.right - clientRect.left) * scale,
      (clientRect.bottom - clientRect.top) * scale,
    }}
    .Rounded<uint32_t>()
    .Clamped(captureSize);
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

PageIndex HWNDPageSource::GetPageCount() const {
  return WGCRenderer::HaveCaptureItem() ? 1 : 0;
}

std::vector<PageID> HWNDPageSource::GetPageIDs() const {
  return {mPageID};
}

std::optional<PreferredSize> HWNDPageSource::GetPreferredSize(PageID) {
  return WGCRenderer::GetPreferredSize();
}

task<void>
HWNDPageSource::RenderPage(RenderContext rc, PageID, PixelRect rect) {
  WGCRenderer::Render(rc.GetRenderTarget(), rect);
  co_return;
}

}// namespace OpenKneeboard
