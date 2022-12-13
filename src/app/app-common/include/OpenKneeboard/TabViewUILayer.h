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

#include <memory>

namespace OpenKneeboard {
struct D2DErrorRenderer;
struct DXResources;

class TabViewUILayer final : public IUILayer {
 public:
  TabViewUILayer(const DXResources& dxr);
  virtual ~TabViewUILayer();

  std::optional<D2D1_POINT_2F> GetCursorPoint() const;

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    const EventContext&,
    const CursorEvent&) override;
  virtual CoordinateMapping GetCoordinateMapping(
    const NextList&,
    const Context&) const override;
  virtual void Render(
    const NextList&,
    const Context&,
    ID2D1DeviceContext*,
    const D2D1_RECT_F&) override;

 private:
  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;
  std::optional<D2D1_POINT_2F> mCursorPoint;
};

}// namespace OpenKneeboard
