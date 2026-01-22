// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include "OpenKneeboard/InterprocessRenderer.hpp"

#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/DCSEvents.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/FooterUILayer.hpp>
#include <OpenKneeboard/KneeboardState.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <algorithm>

namespace OpenKneeboard {

FooterUILayer::FooterUILayer(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard)
  : mDXResources(dxr),
    mKneeboard(kneeboard) {
  AddEventListener(
    kneeboard->evFrameTimerPreEvent,
    std::bind_front(&FooterUILayer::Tick, this));
  AddEventListener(
    kneeboard->evAPIEvent, std::bind_front(&FooterUILayer::OnAPIEvent, this));
  AddEventListener(
    kneeboard->evGameChangedEvent,
    std::bind_front(&FooterUILayer::OnGameChanged, this));

  auto ctx = dxr->mD2DDeviceContext;

  ctx->CreateSolidColorBrush(
    {0.7f, 0.7f, 0.7f, 0.8f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mBackgroundBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mForegroundBrush.put()));
}

FooterUILayer::~FooterUILayer() { this->RemoveAllEventListeners(); }

void FooterUILayer::Tick() {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "FooterUILayer::Tick");
  if (mRenderState == RenderState::Stale) {
    // Already marked dirty, lets' not bother doing it again
    TraceLoggingWriteStop(
      activity,
      "FooterUILayer::Tick",
      TraceLoggingValue("Already dirty", "Result"));
    return;
  }

  const auto now = std::chrono::time_point_cast<Duration>(Clock::now());
  if (now == mLastRenderAt) {
    TraceLoggingWriteStop(
      activity, "FooterUILayer::Tick", TraceLoggingValue("Clean", "Result"));
    return;
  }

  mRenderState = RenderState::Stale;
  evNeedsRepaintEvent.Emit();
  TraceLoggingWriteStop(
    activity,
    "FooterUILayer::Tick",
    TraceLoggingValue("Newly dirty", "Result"));
}

void FooterUILayer::PostCursorEvent(
  const IUILayer::NextList& next,
  const Context& context,
  KneeboardViewID KneeboardViewID,
  const CursorEvent& cursorEvent) {
  this->PostNextCursorEvent(next, context, KneeboardViewID, cursorEvent);
}

IUILayer::Metrics FooterUILayer::GetMetrics(
  const IUILayer::NextList& next,
  const Context& context) const {
  const auto nextMetrics = next.front()->GetMetrics(next.subspan(1), context);

  const auto contentHeight = nextMetrics.mContentArea.mSize.mHeight;
  const auto footerHeight = static_cast<uint32_t>(
    std::lround(contentHeight * (FooterPercent / 100.0f)));
  return Metrics {
    nextMetrics.mPreferredSize.Extended(
      {0, static_cast<uint32_t>(footerHeight)}),
    {{}, nextMetrics.mPreferredSize.mPixelSize},
    nextMetrics.mContentArea,
  };
}

task<void> FooterUILayer::Render(
  const RenderContext& rc,
  const IUILayer::NextList& next,
  const Context& context,
  const PixelRect& rect) {
  OPENKNEEBOARD_TraceLoggingScope("FooterUILayer::Render()");
  mLastRenderSize = rect.mSize;

  const auto tabView = context.mTabView;

  const auto metrics = this->GetMetrics(next, context);
  const auto preferredSize = metrics.mPreferredSize.mPixelSize;

  const auto scale = rect.Height<float>() / preferredSize.mHeight;

  const auto contentHeight = scale * metrics.mContentArea.Height();
  const auto footerHeight = static_cast<uint32_t>(
    std::lround(contentHeight * (FooterPercent / 100.0f)));

  const PixelRect footerRect {
    {rect.Left(), rect.Bottom() - footerHeight},
    {rect.Width(), footerHeight},
  };

  co_await next.front()->Render(
    rc,
    next.subspan(1),
    context,
    {
      rect.mOffset,
      (metrics.mNextArea.mSize.StaticCast<float>() * scale).Rounded<uint32_t>(),
    });

  auto d2d = rc.d2d();
  d2d->FillRectangle(footerRect, mBackgroundBrush.get());

  FLOAT dpix {}, dpiy {};
  d2d->GetDpi(&dpix, &dpiy);
  const auto& dwf = mDXResources->mDWriteFactory;
  winrt::com_ptr<IDWriteTextFormat> clockFormat;
  winrt::check_hresult(dwf->CreateTextFormat(
    FixedWidthUIFont,
    nullptr,
    DWRITE_FONT_WEIGHT_BOLD,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    (footerHeight * 96) / (2 * dpiy),
    L"",
    clockFormat.put()));

  const auto margin = footerHeight / 4;

  const auto now =
    std::chrono::time_point_cast<Duration>(std::chrono::system_clock::now());
  mLastRenderAt = std::chrono::time_point_cast<Duration>(Clock::now());
  mRenderState = RenderState::UpToDate;

  const auto drawClock =
    [&](const std::wstring& clock, DWRITE_TEXT_ALIGNMENT alignment) {
      winrt::com_ptr<IDWriteTextLayout> clockLayout;
      winrt::check_hresult(dwf->CreateTextLayout(
        clock.c_str(),
        static_cast<UINT32>(clock.size()),
        clockFormat.get(),
        float(mLastRenderSize->width - (2 * margin)),
        float(footerHeight),
        clockLayout.put()));
      clockLayout->SetTextAlignment(alignment);
      clockLayout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
      d2d->DrawTextLayout(
        {margin + rect.Left<float>(), rect.Bottom<float>() - footerHeight},
        clockLayout.get(),
        mForegroundBrush.get());
    };

  // Mission time
  if (mMissionTime) {
    const auto missionTime = std::chrono::utc_seconds(*mMissionTime);
    if (mUTCOffset) {
      const auto zuluTime = missionTime - *mUTCOffset;
      // Don't use a dash to separate local from zulu - easy to misread
      // as an offset
      drawClock(
        std::format(L"{:%T} ({:%T}Z)", missionTime, zuluTime),
        DWRITE_TEXT_ALIGNMENT_LEADING);
    } else {
      drawClock(
        std::format(L"{:%T}", missionTime), DWRITE_TEXT_ALIGNMENT_LEADING);
    }
  }

  // Frame count
  if (mKneeboard->GetUISettings().mInGameUI.mFooterFrameCountEnabled) {
    auto ipc = mKneeboard->GetInterprocessRenderer();
    if (ipc) {
      drawClock(
        std::format(L"OKB Frame {}", ipc->GetFrameCountForMetricsOnly()),
        DWRITE_TEXT_ALIGNMENT_CENTER);
    }
  }

  // Real time
  drawClock(
    std::format(
      L"{:%T}", std::chrono::zoned_time(std::chrono::current_zone(), now)),
    DWRITE_TEXT_ALIGNMENT_TRAILING);
}

void FooterUILayer::OnAPIEvent(const APIEvent& ev) {
  if (ev.name == DCSEvents::EVT_SIMULATION_START) {
    const auto mission = ev.ParsedValue<DCSEvents::SimulationStartEvent>();

    const auto startTime = std::chrono::seconds(mission.missionStartTime);

    mMissionTime = startTime;
    return;
  }

  if (ev.name == DCSEvents::EVT_MISSION_TIME) {
    const auto times = ev.TryParsedValue<DCSEvents::MissionTimeEvent>();
    if (!times) {
      dprint("Failed to parse mission times: {}", times.error().what);
      return;
    }
    const auto currentTime =
      std::chrono::seconds(static_cast<uint64_t>(times->currentTime));

    if ((!mMissionTime) || mMissionTime != currentTime) {
      mMissionTime = currentTime;
      mUTCOffset = std::chrono::hours(times->utcOffset);
      mRenderState = RenderState::Stale;
      evNeedsRepaintEvent.Emit();
    }
    return;
  }
}

void FooterUILayer::OnGameChanged(
  DWORD processID,
  const std::filesystem::path&) {
  if (processID == mCurrentGamePID) {
    return;
  }

  mCurrentGamePID = processID;
  mMissionTime = {};
}

}// namespace OpenKneeboard
