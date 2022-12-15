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
#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/FooterUILayer.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/config.h>

#include <algorithm>

namespace OpenKneeboard {

FooterUILayer::FooterUILayer(const DXResources& dxr, KneeboardState* kneeboard)
  : mDXResources(dxr) {
  AddEventListener(
    kneeboard->evFrameTimerPrepareEvent,
    std::bind_front(&FooterUILayer::Tick, this));
  AddEventListener(
    kneeboard->evGameEvent, std::bind_front(&FooterUILayer::OnGameEvent, this));
  AddEventListener(
    kneeboard->evGameChangedEvent,
    std::bind_front(&FooterUILayer::OnGameChanged, this));

  auto ctx = dxr.mD2DDeviceContext;

  ctx->CreateSolidColorBrush(
    {0.7f, 0.7f, 0.7f, 0.8f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mBackgroundBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mForegroundBrush.put()));
}

FooterUILayer::~FooterUILayer() {
  this->RemoveAllEventListeners();
}

void FooterUILayer::Tick() {
  const auto now = std::chrono::time_point_cast<Duration>(Clock::now());
  if (now > mLastRenderAt) {
    evNeedsRepaintEvent.Emit();
  }
}

void FooterUILayer::PostCursorEvent(
  const IUILayer::NextList& next,
  const Context& context,
  const EventContext& eventContext,
  const CursorEvent& cursorEvent) {
  if (!mLastRenderSize) {
    return;
  }
  const auto renderSize = *mLastRenderSize;

  constexpr auto contentRatio = 1 / (1 + (FooterPercent / 100.0f));
  constexpr auto footerRatio = 1 - contentRatio;
  if (cursorEvent.mY > contentRatio) {
    next.front()->PostCursorEvent(next.subspan(1), context, eventContext, {});
    return;
  }

  CursorEvent nextEvent {cursorEvent};
  nextEvent.mY = cursorEvent.mY / contentRatio;
  next.front()->PostCursorEvent(
    next.subspan(1), context, eventContext, nextEvent);
}

IUILayer::Metrics FooterUILayer::GetMetrics(
  const IUILayer::NextList& next,
  const Context& context) const {
  const auto nextMetrics = next.front()->GetMetrics(next.subspan(1), context);

  const auto contentHeight
    = nextMetrics.mContentArea.bottom - nextMetrics.mContentArea.top;
  const auto footerHeight = contentHeight * (FooterPercent / 100.0f);
  return {
    {
      nextMetrics.mCanvasSize.width,
      nextMetrics.mCanvasSize.height + footerHeight,
    },
    {
      nextMetrics.mContentArea.left,
      nextMetrics.mContentArea.top,
      nextMetrics.mContentArea.right,
      nextMetrics.mContentArea.bottom - footerHeight,
    },
  };
}

void FooterUILayer::Render(
  const IUILayer::NextList& next,
  const Context& context,
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& rect) {
  mLastRenderSize = {
    rect.right - rect.left,
    rect.bottom - rect.top,
  };

  const auto tabView = context.mTabView;

  const auto metrics = this->GetMetrics(next, context);
  const auto preferredSize = metrics.mCanvasSize;

  const auto totalHeight = rect.bottom - rect.top;
  const auto scale = totalHeight / preferredSize.height;

  const auto contentHeight
    = scale * (metrics.mContentArea.bottom - metrics.mContentArea.top);
  const auto footerHeight = contentHeight * (FooterPercent / 100.0f);

  const D2D1_RECT_F footerRect {
    rect.left,
    rect.bottom - footerHeight,
    rect.right,
    rect.bottom,
  };

  next.front()->Render(
    next.subspan(1),
    context,
    d2d,
    {rect.left, rect.top, rect.right, rect.bottom - footerHeight});

  d2d->SetTransform(D2D1::Matrix3x2F::Identity());
  d2d->FillRectangle(footerRect, mBackgroundBrush.get());

  FLOAT dpix, dpiy;
  d2d->GetDpi(&dpix, &dpiy);
  const auto& dwf = mDXResources.mDWriteFactory;
  winrt::com_ptr<IDWriteTextFormat> clockFormat;
  winrt::check_hresult(dwf->CreateTextFormat(
    L"Consolas",
    nullptr,
    DWRITE_FONT_WEIGHT_BOLD,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    (footerHeight * 96) / (2 * dpiy),
    L"",
    clockFormat.put()));

  const auto margin = footerHeight / 4;

  const auto now
    = std::chrono::time_point_cast<Duration>(std::chrono::system_clock::now());
  mLastRenderAt = std::chrono::time_point_cast<Duration>(Clock::now());

  const auto drawClock
    = [&](const std::wstring& clock, DWRITE_TEXT_ALIGNMENT alignment) {
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
          {margin, rect.bottom - footerHeight},
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

  // Real time
  drawClock(
    std::format(
      L"{:%T}", std::chrono::zoned_time(std::chrono::current_zone(), now)),
    DWRITE_TEXT_ALIGNMENT_TRAILING);
}

void FooterUILayer::OnGameEvent(const GameEvent& ev) {
  if (ev.name == DCSWorld::EVT_SIMULATION_START) {
    const auto mission = ev.ParsedValue<DCSWorld::SimulationStartEvent>();

    const auto startTime = std::chrono::seconds(mission.missionStartTime);

    mMissionTime = startTime;
    return;
  }

  if (ev.name == DCSWorld::EVT_MISSION_TIME) {
    const auto times = ev.ParsedValue<DCSWorld::MissionTimeEvent>();
    const auto currentTime
      = std::chrono::seconds(static_cast<uint64_t>(times.currentTime));

    if ((!mMissionTime) || mMissionTime != currentTime) {
      mMissionTime = currentTime;
      mUTCOffset = std::chrono::hours(times.utcOffset);
      evNeedsRepaintEvent.Emit();
    }
    return;
  }
}

void FooterUILayer::OnGameChanged(
  DWORD processID,
  const std::shared_ptr<GameInstance>&) {
  if (processID == mCurrentGamePID) {
    return;
  }

  mCurrentGamePID = processID;
  mMissionTime = {};
}

}// namespace OpenKneeboard
