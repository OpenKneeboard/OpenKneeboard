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

#include "IPageSource.h"

namespace OpenKneeboard {

struct CursorEvent;

class IPageSourceWithCursorEvents : public virtual IPageSource {
 public:
  virtual void
  PostCursorEvent(EventContext, const CursorEvent&, PageIndex pageIndex)
    = 0;

  virtual bool CanClearUserInput(PageIndex) const = 0;
  virtual bool CanClearUserInput() const = 0;
  virtual void ClearUserInput(PageIndex) = 0;
  virtual void ClearUserInput() = 0;
};

}// namespace OpenKneeboard
