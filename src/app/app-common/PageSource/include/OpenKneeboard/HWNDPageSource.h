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
  enum class CaptureArea {
    FullWindow,
    ClientArea,
  };
  struct Options {
    CaptureArea mCaptureArea {CaptureArea::FullWindow};
    bool mCaptureCursor {false};

    constexpr auto operator<=>(const Options&) const noexcept = default;
  };

  static std::shared_ptr<HWNDPageSource> Create(
    const DXResources&,
    KneeboardState*,
    HWND window,
    const Options& options) noexcept;

  virtual ~HWNDPageSource();
  static winrt::fire_and_forget final_release(std::unique_ptr<HWNDPageSource>);

  bool HaveWindow() const;
  void InstallWindowHooks(HWND);

  virtual PageIndex GetPageCount() const final override;
  virtual std::vector<PageID> GetPageIDs() const final override;
  virtual D2D1_SIZE_U GetNativeContentSize(PageID) final override;
  virtual ScalingKind GetScalingKind(PageID) final override;

  virtual void RenderPage(
    RenderTargetID,
    ID2D1DeviceContext*,
    PageID,
    const D2D1_RECT_F& rect) final override;

  virtual void PostCursorEvent(EventContext, const CursorEvent&, PageID)
    override final;
  virtual bool CanClearUserInput() const override;
  virtual bool CanClearUserInput(PageID) const override;
  virtual void ClearUserInput(PageID) override;
  virtual void ClearUserInput() override;

  Event<> evWindowClosedEvent;

 private:
  HWNDPageSource() = delete;
  HWNDPageSource(
    const DXResources&,
    KneeboardState*,
    HWND window,
    const Options&);
  winrt::fire_and_forget Init() noexcept;
  void OnFrame();

  winrt::apartment_context mUIThread;
  DXResources mDXR;
  HWND mWindow {};
  Options mOptions {};
  PageID mPageID {};

  // In capture-space coordinates
  std::optional<RECT> GetClientArea() const;
  D3D11_BOX GetContentBox() const;

  struct HookHandles {
    WindowCaptureControl::Handles mHooks64 {};
    winrt::handle mHook32Subprocess;
  };
  std::unordered_map<HWND, HookHandles> mHooks;

  winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice
    mWinRTD3DDevice {nullptr};
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
  size_t mRecreations = 0;
};

}// namespace OpenKneeboard
