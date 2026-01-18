// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/IPageSource.hpp>
#include <OpenKneeboard/IPageSourceWithCursorEvents.hpp>
#include <OpenKneeboard/WGCRenderer.hpp>
#include <OpenKneeboard/WindowCaptureControl.hpp>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/task.hpp>

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

class HWNDPageSource final : public WGCRenderer,
                             public virtual IPageSourceWithCursorEvents,
                             public virtual EventReceiver {
 public:
  enum class CaptureArea {
    FullWindow,
    ClientArea,
  };
  struct Options : WGCRenderer::Options {
    CaptureArea mCaptureArea {CaptureArea::FullWindow};

    constexpr bool operator==(const Options&) const noexcept = default;
  };

  static task<std::shared_ptr<HWNDPageSource>> Create(
    audited_ptr<DXResources>,
    KneeboardState*,
    HWND window,
    Options options) noexcept;

  virtual ~HWNDPageSource();
  virtual task<void> DisposeAsync() noexcept override;

  bool HaveWindow() const;
  void InstallWindowHooks(HWND);

  virtual void PostCursorEvent(KneeboardViewID, const CursorEvent&, PageID)
    override final;
  virtual bool CanClearUserInput() const override;
  virtual bool CanClearUserInput(PageID) const override;
  virtual void ClearUserInput(PageID) override;
  virtual void ClearUserInput() override;

  virtual PageIndex GetPageCount() const override;
  virtual std::vector<PageID> GetPageIDs() const override;
  virtual std::optional<PreferredSize> GetPreferredSize(PageID) override;
  task<void> RenderPage(RenderContext, PageID, PixelRect rect) override;

  Event<> evWindowClosedEvent;

 protected:
  virtual std::optional<float> GetHDRWhiteLevelInNits() const override;
  virtual winrt::Windows::Graphics::DirectX::DirectXPixelFormat GetPixelFormat()
    const override;
  virtual winrt::Windows::Graphics::Capture::GraphicsCaptureItem
  CreateWGCaptureItem() override;
  virtual PixelRect GetContentRect(const PixelSize& captureSize) const override;
  virtual PixelSize GetSwapchainDimensions(
    const PixelSize& captureSize) const override;

 private:
  using WGCRenderer::Render;
  HWNDPageSource() = delete;
  HWNDPageSource(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    HWND window,
    const Options&);
  DisposalState mDisposal;
  [[nodiscard]]
  task<void> Init() noexcept;
  OpenKneeboard::fire_and_forget InitializeInputHook() noexcept;

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
  mutable size_t mRecreations = 0;

  FLOAT mSDRWhiteLevelInNits = D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL;
  bool mIsHDR {false};
  winrt::Windows::Graphics::DirectX::DirectXPixelFormat mPixelFormat;

  PageID mPageID;
};

}// namespace OpenKneeboard
