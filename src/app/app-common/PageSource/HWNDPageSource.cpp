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

static UINT gControlMessage;

std::shared_ptr<HWNDPageSource> HWNDPageSource::Create(
  const audited_ptr<DXResources>& dxr,
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

winrt::fire_and_forget HWNDPageSource::Init() noexcept {
  WGCPageSource::Init();
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

        dprintf(
          L"Capturing on monitor '{}' connected to adapter '{}' (LUID {:#x})",
          std::wstring_view {outputDesc.DeviceName},
          std::wstring_view {adapterDesc.Description},
          std::bit_cast<uint64_t>(adapterDesc.AdapterLuid));
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
      mWindow,
      winrt::guid_of<WGC::GraphicsCaptureItem>(),
      winrt::put_abi(item)));
    LogAdapter(mWindow);
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
      return nullptr;
    }
    const auto monitor = MonitorFromWindow(mWindow, MONITOR_DEFAULTTONULL);
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
      dprintf(
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
    const auto monitor = MonitorFromWindow(mWindow, MONITOR_DEFAULTTONULL);
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
    this->mWindow = {};
    this->evWindowClosedEvent.Emit();
  });
  this->InitializeInputHook();
  return item;
}

winrt::fire_and_forget HWNDPageSource::InitializeInputHook() noexcept {
  co_await winrt::resume_after(std::chrono::milliseconds(100));
  PostMessageW(
    mWindow,
    gControlMessage,
    static_cast<WPARAM>(WindowCaptureControl::WParam::Initialize),
    reinterpret_cast<LPARAM>(mWindow));
}

HWNDPageSource::HWNDPageSource(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard,
  HWND window,
  const Options& options)
  : WGCPageSource(dxr, kneeboard, options),
    mDXR(dxr),
    mWindow(window),
    mOptions(options) {
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

  WGCPageSource::final_release(std::move(p));
  co_return;
}

PixelRect HWNDPageSource::GetContentRect(const PixelSize& captureSize) {
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

winrt::Windows::Foundation::IAsyncAction
HWNDPageSource::InitializeInCaptureThread() {
  co_return;
}

PixelSize HWNDPageSource::GetSwapchainDimensions(const PixelSize& contentSize) {
  // Don't create massive buffers if it just moves between a few fixed
  // sizes, but create full-screen buffers for smoothness if it's being
  // resized a bunch
  if (++mRecreations <= 10) {
    return contentSize;
  }
  const auto monitor = MonitorFromWindow(mWindow, MONITOR_DEFAULTTONULL);
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
  dprintf(L"Window capture monitor is {}x{}", monitorWidth, monitorHeight);
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
  EventContext,
  const CursorEvent& ev,
  PageID) {
  if (!mWindow) {
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

std::optional<PixelRect> HWNDPageSource::GetClientArea(
  const PixelSize& captureSize) const {
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
    .Rounded<uint32_t>();
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
