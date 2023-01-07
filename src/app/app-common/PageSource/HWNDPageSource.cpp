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
#include <OpenKneeboard/weak_wrap.h>
#include <Windows.Graphics.Capture.Interop.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <libloaderapi.h>
#include <shellapi.h>
#include <wil/cppwinrt.h>
#include <wil/cppwinrt_helpers.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.System.h>
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
  if (!ret->HaveWindow()) {
    return nullptr;
  }
  ret->Init();
  return ret;
}

winrt::fire_and_forget HWNDPageSource::Init() noexcept {
  const auto keepAlive = shared_from_this();
  co_await wil::resume_foreground(mDQC.DispatcherQueue());
  {
    const auto d2dLock = mDXR.AcquireLock();

    auto wgiFactory = winrt::get_activation_factory<WGC::GraphicsCaptureItem>();
    auto interopFactory = wgiFactory.as<IGraphicsCaptureItemInterop>();
    WGC::GraphicsCaptureItem item {nullptr};
    winrt::check_hresult(interopFactory->CreateForWindow(
      mWindow,
      winrt::guid_of<WGC::GraphicsCaptureItem>(),
      winrt::put_abi(item)));

    winrt::com_ptr<IInspectable> inspectable {nullptr};
    WGDX::Direct3D11::IDirect3DDevice winrtD3D {nullptr};
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(

      mDXR.mDXGIDevice.get(),
      reinterpret_cast<IInspectable**>(winrt::put_abi(winrtD3D))));

    mFramePool = WGC::Direct3D11CaptureFramePool::Create(
      winrtD3D,
      WGDX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
      2,
      item.Size());
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

  co_await std::chrono::milliseconds(100);
  static auto sControlMessage
    = RegisterWindowMessageW(WindowCaptureControl::WindowMessageName);
  PostMessageW(
    mWindow,
    sControlMessage,
    static_cast<WPARAM>(WindowCaptureControl::WParam::Initialize),
    reinterpret_cast<LPARAM>(mWindow));
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
  co_await p->mDQC.DispatcherQueue();
  p->mCaptureSession.Close();
  p->mFramePool.Close();
  p->mCaptureSession = {nullptr};
  p->mCaptureItem = {nullptr};
  p->mFramePool = {nullptr};

  co_await p->mUIThread;
  co_await p->mDQC.ShutdownQueueAsync();
  p->mDQC = {nullptr};
  const auto lock = p->mDXR.AcquireLock();
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

  auto lock = mDXR.AcquireLock();
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
  ctx->CopyResource(mTexture.get(), d3dSurface.get());
  mNeedsRepaint = true;
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
  dprintf("ClientOrigin: ({} , {})", clientLeft, clientTop);

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
  PageIndex) {
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

  static auto sControlMessage
    = RegisterWindowMessageW(WindowCaptureControl::WindowMessageName);

  SendMessageW(
    target,
    sControlMessage,
    static_cast<WPARAM>(WindowCaptureControl::WParam::StartInjection),
    reinterpret_cast<LPARAM>(mWindow));
  const scope_guard unhook([=]() {
    SendMessageW(
      target,
      sControlMessage,
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

bool HWNDPageSource::CanClearUserInput(PageIndex) const {
  return false;
}

void HWNDPageSource::ClearUserInput(PageIndex) {
  // nothing to do here
}

void HWNDPageSource::ClearUserInput() {
  // nothing to do here
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

  mHooks[target].mHook32Subprocess = {shellExecuteInfo.hProcess};
}

}// namespace OpenKneeboard
