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

#include <OpenKneeboard/IUILayer.h>

#include <OpenKneeboard/audited_ptr.h>

#include <memory>

namespace OpenKneeboard {
class D2DErrorRenderer;
struct DXResources;

class TabViewUILayer final : public IUILayer {
 public:
  TabViewUILayer(const audited_ptr<DXResources>& dxr);
  virtual ~TabViewUILayer();

  std::optional<D2D1_POINT_2F> GetCursorPoint() const;

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    const EventContext&,
    const CursorEvent&) override;
  virtual Metrics GetMetrics(const NextList&, const Context&) const override;
  virtual void Render(
    const RenderContext&,
    const NextList&,
    const Context&,
    const PixelRect&) override;

 private:
  void RenderError(
    ID2D1DeviceContext*,
    std::string_view text,
    const PixelRect& rect);

  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;
  std::optional<D2D1_POINT_2F> mCursorPoint;
  winrt::com_ptr<ID2D1SolidColorBrush> mErrorBackgroundBrush;
};

}// namespace OpenKneeboard
