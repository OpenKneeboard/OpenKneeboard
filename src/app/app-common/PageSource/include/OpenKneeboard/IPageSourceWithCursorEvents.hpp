// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include "IPageSource.hpp"
#include "IPageSourceWithInternalCaching.hpp"

#include <OpenKneeboard/Events.hpp>

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
class IPageSourceWithCursorEvents
  : public virtual IPageSourceWithInternalCaching {
 public:
  virtual void PostCursorEvent(KneeboardViewID, const CursorEvent&, PageID) = 0;

  virtual bool CanClearUserInput(PageID) const = 0;
  virtual bool CanClearUserInput() const = 0;
  virtual void ClearUserInput(PageID) = 0;
  virtual void ClearUserInput() = 0;
};

}// namespace OpenKneeboard
