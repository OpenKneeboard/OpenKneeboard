// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/SHM.hpp>
#include <OpenKneeboard/UILayerBase.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

#include <shims/winrt/base.h>

#include <chrono>
#include <memory>

namespace OpenKneeboard {

class KneeboardState;
struct APIEvent;
struct GameInstance;

class FooterUILayer final : public UILayerBase, private EventReceiver {
 public:
  FooterUILayer(const audited_ptr<DXResources>& dxr, KneeboardState*);
  virtual ~FooterUILayer();

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    KneeboardViewID,
    const CursorEvent&) override;
  virtual Metrics GetMetrics(const NextList&, const Context&) const override;
  [[nodiscard]] task<void> Render(
    const RenderContext&,
    const NextList&,
    const Context&,
    const PixelRect&) override;

 private:
  void Tick();
  void OnAPIEvent(const APIEvent&);
  void OnGameChanged(DWORD processID, const std::filesystem::path&);

  audited_ptr<DXResources> mDXResources;
  winrt::com_ptr<ID2D1Brush> mBackgroundBrush;
  winrt::com_ptr<ID2D1Brush> mForegroundBrush;
  std::optional<D2D1_SIZE_F> mLastRenderSize;

  DWORD mCurrentGamePID {};
  std::optional<std::chrono::seconds> mMissionTime;
  std::optional<std::chrono::seconds> mUTCOffset;

  KneeboardState* mKneeboard {nullptr};

  using Clock = std::chrono::system_clock;
  using Duration = std::chrono::seconds;
  using TimePoint = std::chrono::time_point<Clock, Duration>;

  enum class RenderState {
    UpToDate,
    Stale,
  };
  TimePoint mLastRenderAt {};
  RenderState mRenderState {RenderState::Stale};
};

}// namespace OpenKneeboard
