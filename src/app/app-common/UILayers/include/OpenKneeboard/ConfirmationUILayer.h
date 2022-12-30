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

namespace OpenKneeboard {

struct CursorEvent;
class IToolbarItemWithConfirmation;

class ConfirmationUILayer final : public IUILayer {
 public:
  ConfirmationUILayer(const std::shared_ptr<IToolbarItemWithConfirmation>&);
  virtual ~ConfirmationUILayer();

  virtual void PostCursorEvent(
    const NextList&,
    const Context&,
    const EventContext&,
    const CursorEvent&) override;

  virtual void Render(
    const NextList&,
    const Context&,
    ID2D1DeviceContext*,
    const D2D1_RECT_F&) override;

  virtual Metrics GetMetrics(const NextList&, const Context&) const override;

  ConfirmationUILayer() = delete;

 private:
  std::shared_ptr<IToolbarItemWithConfirmation> mItem;
  std::optional<D2D1_RECT_F> mRect;
};

}// namespace OpenKneeboard
