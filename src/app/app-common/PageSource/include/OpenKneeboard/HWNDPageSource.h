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
#include <shims/winrt/base.h>
#include <winrt/Windows.Graphics.Capture.h>

namespace OpenKneeboard {

class KneeboardState;

class HWNDPageSource final : public virtual IPageSource,
                             public IPageSourceWithCursorEvents,
                             public EventReceiver {
 public:
  HWNDPageSource() = delete;
  HWNDPageSource(const DXResources&, KneeboardState*, HWND window);
  virtual ~HWNDPageSource();

  virtual PageIndex GetPageCount() const final override;
  virtual D2D1_SIZE_U GetNativeContentSize(PageIndex pageIndex) final override;

  virtual void RenderPage(
    ID2D1DeviceContext*,
    PageIndex pageIndex,
    const D2D1_RECT_F& rect) final override;

  virtual void
  PostCursorEvent(EventContext, const CursorEvent&, PageIndex pageIndex) final;

 private:
  void OnFrame();

  DXResources mDXR;
  HWND mWindow {};

  winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool mFramePool {
    nullptr};
  winrt::Windows::Graphics::Capture::GraphicsCaptureSession mCaptureSession {
    nullptr};

  std::mutex mTextureMutex;
  D2D1_SIZE_U mContentSize {};
  winrt::com_ptr<ID3D11Texture2D> mTexture;
  winrt::com_ptr<ID2D1Bitmap1> mBitmap;
  bool mNeedsRepaint {true};
  uint32_t mMouseButtons {};
};

}// namespace OpenKneeboard
