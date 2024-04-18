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
#include <OpenKneeboard/WGCPageSource.h>
#include <OpenKneeboard/WindowCaptureControl.h>

#include <OpenKneeboard/audited_ptr.h>
#include <OpenKneeboard/handles.h>

#include <shims/winrt/base.h>

#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.System.h>

#include <memory>

#include <SetupAPI.h>

namespace OpenKneeboard {

class KneeboardState;

namespace D3D11 {
class SpriteBatch;
}

class HWNDPageSource final : public WGCPageSource,
                             public virtual IPageSourceWithCursorEvents,
                             public virtual EventReceiver {
 public:
  enum class CaptureArea {
    FullWindow,
    ClientArea,
  };
  struct Options : WGCPageSource::Options {
    CaptureArea mCaptureArea {CaptureArea::FullWindow};

    constexpr bool operator==(const Options&) const noexcept = default;
  };

  static std::shared_ptr<HWNDPageSource> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    HWND window,
    const Options& options) noexcept;

  virtual ~HWNDPageSource();
  static winrt::fire_and_forget final_release(std::unique_ptr<HWNDPageSource>);

  bool HaveWindow() const;
  void InstallWindowHooks(HWND);

  virtual void PostCursorEvent(EventContext, const CursorEvent&, PageID)
    override final;
  virtual bool CanClearUserInput() const override;
  virtual bool CanClearUserInput(PageID) const override;
  virtual void ClearUserInput(PageID) override;
  virtual void ClearUserInput() override;

  Event<> evWindowClosedEvent;

 protected:
  virtual std::optional<float> GetHDRWhiteLevelInNits() const override;
  virtual winrt::Windows::Graphics::DirectX::DirectXPixelFormat GetPixelFormat()
    const override;
  virtual winrt::Windows::Graphics::Capture::GraphicsCaptureItem
  CreateWGCaptureItem() override;
  virtual PixelRect GetContentRect(const PixelSize& captureSize) override;
  virtual PixelSize GetSwapchainDimensions(
    const PixelSize& captureSize) override;
  virtual winrt::Windows::Foundation::IAsyncAction InitializeContentToCapture()
    override;

 private:
  HWNDPageSource() = delete;
  HWNDPageSource(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    HWND window,
    const Options&);
  winrt::fire_and_forget Init() noexcept;
  winrt::fire_and_forget InitializeInputHook() noexcept;

  void LogAdapter(HMONITOR);
  void LogAdapter(HWND);

  const audited_ptr<DXResources> mDXR;

  winrt::apartment_context mUIThread;
  HWND mCaptureWindow {};
  HWND mInputWindow {};
  Options mOptions {};

  // In capture-space coordinates
  std::optional<PixelRect> GetClientArea(const PixelSize& captureSize) const;

  struct HookHandles {
    WindowCaptureControl::Handles mHooks64 {};
    winrt::handle mHook32Subprocess;
  };
  std::unordered_map<HWND, HookHandles> mHooks;

  uint32_t mMouseButtons {};
  size_t mRecreations = 0;

  FLOAT mSDRWhiteLevelInNits = D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL;
  bool mIsHDR {false};
  winrt::Windows::Graphics::DirectX::DirectXPixelFormat mPixelFormat;
};

}// namespace OpenKneeboard
