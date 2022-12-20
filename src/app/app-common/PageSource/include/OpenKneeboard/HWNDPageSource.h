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
#include <OpenKneeboard/IPageSourceWithCursorEvents.h>
#include <OpenKneeboard/WindowCaptureControl.h>
#include <OpenKneeboard/handles.h>
#include <shims/winrt/base.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.System.h>

#include <memory>

namespace OpenKneeboard {

class KneeboardState;

class HWNDPageSource final
  : public virtual IPageSource,
    public IPageSourceWithCursorEvents,
    public EventReceiver,
    public std::enable_shared_from_this<HWNDPageSource> {
 public:
  static std::shared_ptr<HWNDPageSource>
  Create(const DXResources&, KneeboardState*, HWND window) noexcept;

  virtual ~HWNDPageSource();
  static winrt::fire_and_forget final_release(std::unique_ptr<HWNDPageSource>);

  bool HaveWindow() const;

  virtual PageIndex GetPageCount() const final override;
  virtual D2D1_SIZE_U GetNativeContentSize(PageIndex pageIndex) final override;

  virtual void RenderPage(
    ID2D1DeviceContext*,
    PageIndex pageIndex,
    const D2D1_RECT_F& rect) final override;

  virtual void
  PostCursorEvent(EventContext, const CursorEvent&, PageIndex pageIndex) final;

  Event<> evWindowClosedEvent;

 private:
  HWNDPageSource() = delete;
  HWNDPageSource(const DXResources&, KneeboardState*, HWND window);
  void InitializeOnWorkerThread() noexcept;
  void OnFrame() noexcept;
  void InstallWindowHooks(HWND);

  winrt::apartment_context mUIThread;
  DXResources mDXR;
  HWND mWindow {};
  struct HookHandles {
    WindowCaptureControl::Handles mHooks64 {};
    winrt::handle mHook32Subprocess;
  };
  std::unordered_map<HWND, HookHandles> mHooks;

  winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool mFramePool {
    nullptr};
  winrt::Windows::Graphics::Capture::GraphicsCaptureSession mCaptureSession {
    nullptr};
  winrt::Windows::Graphics::Capture::GraphicsCaptureItem mCaptureItem {nullptr};

  winrt::Windows::System::DispatcherQueueController mDQC {nullptr};

  D2D1_SIZE_U mContentSize {};
  winrt::com_ptr<ID3D11Texture2D> mTexture;
  winrt::com_ptr<ID2D1Bitmap1> mBitmap;
  bool mNeedsRepaint {true};
  uint32_t mMouseButtons {};
};

}// namespace OpenKneeboard
