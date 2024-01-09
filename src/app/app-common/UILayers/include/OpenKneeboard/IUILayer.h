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

#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/IKneeboardView.h>
#include <OpenKneeboard/ITabView.h>
#include <OpenKneeboard/RenderTarget.h>

#include <memory>
#include <span>

#include <d2d1_1.h>

namespace OpenKneeboard {

struct CursorEvent;

class IUILayer {
 public:
  IUILayer();
  virtual ~IUILayer();

  using NextList = std::span<IUILayer*>;

  struct Context {
    const std::shared_ptr<ITabView>& mTabView {nullptr};
    const std::shared_ptr<IKneeboardView>& mKneeboardView {nullptr};
    bool mIsActiveForInput {false};
  };

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    const EventContext&,
    const CursorEvent&)
    = 0;

  virtual void
  Render(RenderTarget*, const NextList&, const Context&, const D2D1_RECT_F&)
    = 0;

  struct Metrics {
    D2D1_SIZE_F mCanvasSize {};
    D2D1_RECT_F mNextArea {};
    D2D1_RECT_F mContentArea {};
    ScalingKind mScalingKind {};
  };

  virtual Metrics GetMetrics(const NextList&, const Context&) const = 0;

  Event<> evNeedsRepaintEvent;
};

}// namespace OpenKneeboard
