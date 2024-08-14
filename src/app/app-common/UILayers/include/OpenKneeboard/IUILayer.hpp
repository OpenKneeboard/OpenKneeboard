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

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/RenderTarget.hpp>
#include <OpenKneeboard/TabView.hpp>

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
    const std::shared_ptr<TabView>& mTabView {nullptr};
    const std::shared_ptr<KneeboardView>& mKneeboardView {nullptr};
    bool mIsActiveForInput {false};
  };

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    KneeboardViewID,
    const CursorEvent&)
    = 0;

  [[nodiscard]] virtual IAsyncAction Render(
    const RenderContext&,
    const NextList&,
    const Context&,
    const PixelRect&)
    = 0;

  struct Metrics {
    PreferredSize mPreferredSize {};
    PixelRect mNextArea {};
    PixelRect mContentArea {};

    Metrics() = delete;
    constexpr Metrics(
      PreferredSize preferredSize,
      PixelRect nextArea,
      PixelRect contentArea)
      : mPreferredSize(preferredSize),
        mNextArea(nextArea),
        mContentArea(contentArea) {
    }
  };

  virtual Metrics GetMetrics(const NextList&, const Context&) const = 0;

  Event<> evNeedsRepaintEvent;
};

}// namespace OpenKneeboard
