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
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/HWNDPageSource.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/WindowCaptureControl.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/handles.h>
#include <OpenKneeboard/scope_guard.h>
#include <Windows.Graphics.Capture.Interop.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <libloaderapi.h>
#include <shellapi.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <wow64apiset.h>

namespace WGC = winrt::Windows::Graphics::Capture;
namespace WGDX = winrt::Windows::Graphics::DirectX;

// TODO: error handling :)

namespace OpenKneeboard {

static std::unordered_map<HWND, HWNDPageSource*> gInstances;
static unique_hhook gHook;

std::shared_ptr<HWNDPageSource> HWNDPageSource::Create(
  const DXResources& dxr,
  KneeboardState* kneeboard,
  HWND window) noexcept {
  static_assert(with_final_release<HWNDPageSource>);
  auto ret
    = shared_with_final_release(new HWNDPageSource(dxr, kneeboard, window));
  if (ret->HaveWindow()) {
    return ret;
  }
  return nullptr;
}

void HWNDPageSource::InitializeOnWorkerThread() noexcept {
  auto wgiFactory = winrt::get_activation_factory<WGC::GraphicsCaptureItem>();
  auto interopFactory = wgiFactory.as<IGraphicsCaptureItemInterop>();
  WGC::GraphicsCaptureItem item {nullptr};
  winrt::check_hresult(interopFactory->CreateForWindow(
    mWindow, winrt::guid_of<WGC::GraphicsCaptureItem>(), winrt::put_abi(item)));

  winrt::com_ptr<IInspectable> inspectable {nullptr};
  WGDX::Direct3D11::IDirect3DDevice winrtD3D {nullptr};
  winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(

    mDXR.mDXGIDevice.get(),
    reinterpret_cast<IInspectable**>(winrt::put_abi(winrtD3D))));

  auto lock = mDXR.AcquireD2DLockout();
  mFramePool = WGC::Direct3D11CaptureFramePool::Create(
    winrtD3D, WGDX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, item.Size());
  mFramePool.FrameArrived(
    [this](const auto&, const auto&) { this->OnFrame(); });

  mCaptureSession = mFramePool.CreateCaptureSession(item);
  mCaptureSession.IsCursorCaptureEnabled(false);
  mCaptureSession.StartCapture();

  mCaptureItem = item;
  mCaptureItem.Closed([this](auto&, auto&) {
    this->mWindow = {};
    this->evWindowClosedEvent.Emit();
  });
}

HWNDPageSource::HWNDPageSource(
  const DXResources& dxr,
  KneeboardState* kneeboard,
  HWND window)
  : mDXR(dxr), mWindow(window) {
  if (!window) {
    return;
  }
  if (!WGC::GraphicsCaptureSession::IsSupported()) {
    return;
  }

  AddEventListener(kneeboard->evFrameTimerPrepareEvent, [this]() {
    if (mNeedsRepaint) {
      evContentChangedEvent.Emit(ContentChangeType::Modified);
    }
  });

  mDQC = winrt::Windows::System::DispatcherQueueController::
    CreateOnDedicatedThread();
  mDQC.DispatcherQueue().TryEnqueue(
    [this]() { this->InitializeOnWorkerThread(); });

  this->InstallWindowHooks();
}

bool HWNDPageSource::HaveWindow() const {
  return static_cast<bool>(mWindow);
}

// Destruction is handled in final_release instead
HWNDPageSource::~HWNDPageSource() = default;

winrt::fire_and_forget HWNDPageSource::final_release(
  std::unique_ptr<HWNDPageSource> p) {
  if (p->mHook32Subprocess) {
    TerminateProcess(p->mHook32Subprocess.get(), 0);
    p->mHook32Subprocess = {};
  }

  p->RemoveAllEventListeners();
  if (!p->mDQC) {
    co_await p->mUIThread;
    co_return;
  }

  // Switch to DQ thread to clean up the Windows.Graphics.Capture objects that
  // were created in that thread
  co_await p->mDQC.DispatcherQueue();
  p->mCaptureSession.Close();
  p->mFramePool.Close();
  p->mCaptureSession = {nullptr};
  p->mCaptureItem = {nullptr};
  p->mFramePool = {nullptr};

  co_await p->mUIThread;
  co_await p->mDQC.ShutdownQueueAsync();
  p->mDQC = {nullptr};
  const auto lock = p->mDXR.AcquireD2DLockout();
  p->mTexture = nullptr;
  p->mBitmap = nullptr;
}

PageIndex HWNDPageSource::GetPageCount() const {
  if (this->HaveWindow()) {
    return 1;
  }
  return 0;
}

D2D1_SIZE_U HWNDPageSource::GetNativeContentSize(PageIndex) {
  if (!mBitmap) {
    return {};
  }
  return {mContentSize.width, mContentSize.height};
}

void HWNDPageSource::RenderPage(
  ID2D1DeviceContext* ctx,
  PageIndex index,
  const D2D1_RECT_F& rect) {
  if (!mBitmap) {
    return;
  }
  if (!this->HaveWindow()) {
    return;
  }

  ctx->DrawBitmap(
    mBitmap.get(),
    rect,
    1.0f,
    D2D1_INTERPOLATION_MODE_ANISOTROPIC,
    D2D1_RECT_F {
      0.0f,
      0.0f,
      static_cast<FLOAT>(mContentSize.width),
      static_cast<FLOAT>(mContentSize.height),
    });
  mNeedsRepaint = false;
}

void HWNDPageSource::OnFrame() noexcept {
  auto frame = mFramePool.TryGetNextFrame();
  if (!frame) {
    return;
  }
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

  winrt::com_ptr<ID3D11DeviceContext> ctx;
  mDXR.mD3DDevice->GetImmediateContext(ctx.put());

  if (mTexture) {
    D3D11_TEXTURE2D_DESC desc {};
    mTexture->GetDesc(&desc);
    if (surfaceDesc.Width != desc.Width || surfaceDesc.Height != desc.Height) {
      mTexture = nullptr;
      mBitmap = nullptr;
    }
  }
  if (!mTexture) {
    winrt::check_hresult(
      mDXR.mD3DDevice->CreateTexture2D(&surfaceDesc, nullptr, mTexture.put()));
    winrt::check_hresult(mDXR.mD2DDeviceContext->CreateBitmapFromDxgiSurface(
      mTexture.as<IDXGISurface>().get(), nullptr, mBitmap.put()));
  }

  mContentSize = {
    static_cast<UINT>(contentSize.Width),
    static_cast<UINT>(contentSize.Height)};
  auto lock = mDXR.AcquireD2DLockout();
  ctx->CopyResource(mTexture.get(), d3dSurface.get());
  mNeedsRepaint = true;
}

void HWNDPageSource::PostCursorEvent(
  EventContext,
  const CursorEvent& ev,
  PageIndex) {
  if (!mTexture) {
    return;
  }

  // The event point should already be scaled to native content size
  POINT point {std::lround(ev.mX), std::lround(ev.mY)};
  const auto target = mWindow;
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
      PostMessage(mWindow, WM_LBUTTONUP, wParam, lParam);
    }
    if (mMouseButtons & (1 << 1)) {
      PostMessage(mWindow, WM_RBUTTONUP, wParam, lParam);
    }
    mMouseButtons = {};
    PostMessage(mWindow, WM_MOUSELEAVE, 0, 0);
    return;
  }

  static auto sControlMessage
    = RegisterWindowMessageW(WindowCaptureControl::WindowMessageName);

  // We only pay attention to buttons (1) and (1 << 1)
  const auto buttons = ev.mButtons & 3;
  if (buttons == mMouseButtons) {
    SendMessage(
      target,
      sControlMessage,
      static_cast<WPARAM>(WindowCaptureControl::WParam::Disable_WM_MOUSELEAVE),
      0);
    SendMessage(target, WM_MOUSEMOVE, wParam, lParam);
    SendMessage(
      target,
      sControlMessage,
      static_cast<WPARAM>(WindowCaptureControl::WParam::Enable_WM_MOUSELEAVE),
      0);
    return;
  }

  const scope_guard restoreFgWindow(
    [window = GetForegroundWindow()]() { SetForegroundWindow(window); });

  const auto down = (ev.mButtons & ~mMouseButtons);
  const auto up = (mMouseButtons & ~ev.mButtons);
  mMouseButtons = buttons;

  if (down & 1) {
    SendMessage(target, WM_LBUTTONDOWN, wParam, lParam);
  }
  if (up & 1) {
    SendMessage(target, WM_LBUTTONUP, wParam, lParam);
  }
  if (down & (1 << 1)) {
    SendMessage(target, WM_RBUTTONDOWN, wParam, lParam);
  }
  if (up & (1 << 1)) {
    SendMessage(target, WM_RBUTTONUP, wParam, lParam);
  }
}

void HWNDPageSource::InstallWindowHooks() {
  BOOL is32Bit {FALSE};
  {
    DWORD processID {};
    GetWindowThreadProcessId(mWindow, &processID);
    winrt::handle process {
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, processID)};
    if (!process) {
      return;
    }
    winrt::check_bool(IsWow64Process(process.get(), &is32Bit));
  }

  if (!is32Bit) {
    // Natively use SetWindowsHookEx()
    mHooks64 = WindowCaptureControl::InstallHooks(mWindow);
    return;
  }

  // We need a 32-bit subprocess to install our hook

  const auto helper = (Filesystem::GetRuntimeDirectory()
                       / RuntimeFiles::WINDOW_CAPTURE_HOOK_32BIT_HELPER)
                        .wstring();
  const auto commandLine = std::format(
    L"{} {}",
    reinterpret_cast<uint64_t>(mWindow),
    static_cast<uint64_t>(GetCurrentProcessId()));

  SHELLEXECUTEINFOW shellExecuteInfo {
    .cbSize = sizeof(SHELLEXECUTEINFOW),
    .fMask = SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS,
    .lpVerb = L"open",
    .lpFile = helper.c_str(),
    .lpParameters = commandLine.c_str(),
  };
  winrt::check_bool(ShellExecuteExW(&shellExecuteInfo));

  mHook32Subprocess.attach(shellExecuteInfo.hProcess);
}

}// namespace OpenKneeboard
