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

#include "IPageSource.h"

#include <OpenKneeboard/Events.h>

namespace OpenKneeboard {

struct CursorEvent;

/** A page source that handles cursor events itself.
 *
 * This implies:
 * - it does its' own caching, if needed, as it is likely to need to manage
 *   multiple layers of content depending on the cursor position, e.g. hover
 *   markers for hyperlink over a separately cached content layer.
 * - it handles doodles itself, if desired
 */
class IPageSourceWithCursorEvents : public virtual IPageSource {
 public:
  virtual void PostCursorEvent(EventContext, const CursorEvent&, PageID) = 0;

  virtual bool CanClearUserInput(PageID) const = 0;
  virtual bool CanClearUserInput() const = 0;
  virtual void ClearUserInput(PageID) = 0;
  virtual void ClearUserInput() = 0;
};

}// namespace OpenKneeboard
