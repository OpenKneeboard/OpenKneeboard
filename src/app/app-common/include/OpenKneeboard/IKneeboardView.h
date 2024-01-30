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

#include <OpenKneeboard/Bookmark.h>
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/ITab.h>

#include <OpenKneeboard/inttypes.h>

#include <memory>

#include <d2d1.h>

namespace OpenKneeboard {

enum class UserAction;
class ITabView;
struct CursorEvent;

struct KneeboardViewID final : public UniqueIDBase<KneeboardViewID> {};

enum class CursorPositionState {
  IN_CONTENT_RECT,
  IN_CANVAS_RECT,
  NO_CURSOR_POSITION
};

class IKneeboardView {
 public:
  IKneeboardView();
  virtual ~IKneeboardView();

  virtual winrt::guid GetPersistentGUID() const = 0;
  // TODO: now that we have persistent GUIDs, we could just use them instead and
  // get rid of these.
  virtual KneeboardViewID GetRuntimeID() const = 0;

  virtual std::shared_ptr<ITabView> GetCurrentTabView() const = 0;
  virtual std::shared_ptr<ITab> GetCurrentTab() const = 0;
  virtual TabIndex GetTabIndex() const = 0;
  virtual std::shared_ptr<ITabView> GetTabViewByID(ITab::RuntimeID) const = 0;
  virtual void SetCurrentTabByIndex(TabIndex) = 0;
  virtual void SetCurrentTabByRuntimeID(ITab::RuntimeID) = 0;

  virtual std::vector<Bookmark> GetBookmarks() const = 0;
  virtual void RemoveBookmark(const Bookmark&) = 0;
  virtual void GoToBookmark(const Bookmark&) = 0;

  virtual std::optional<Bookmark> AddBookmarkForCurrentPage() = 0;
  virtual void RemoveBookmarkForCurrentPage() = 0;
  virtual bool CurrentPageHasBookmark() const = 0;

  virtual void GoToPreviousBookmark() = 0;
  virtual void GoToNextBookmark() = 0;

  virtual void PreviousTab() = 0;
  virtual void NextTab() = 0;

  virtual PixelSize GetIPCRenderSize() const = 0;
  /// ContentRenderRect may be scaled; this is the 'real' size.
  virtual PreferredSize GetPreferredSize() const = 0;

  Event<TabIndex> evCurrentTabChangedEvent;
  // TODO - cursor and repaint?
  Event<> evNeedsRepaintEvent;
  Event<CursorEvent> evCursorEvent;
  Event<> evLayoutChangedEvent;
  Event<> evBookmarksChangedEvent;

  virtual void RenderWithChrome(
    RenderTarget*,
    const D2D1_RECT_F& rect,
    bool isActiveForInput) noexcept
    = 0;

  virtual std::optional<D2D1_POINT_2F> GetCursorCanvasPoint() const = 0;
  virtual std::optional<D2D1_POINT_2F> GetCursorContentPoint() const = 0;
  virtual D2D1_POINT_2F GetCursorCanvasPoint(
    const D2D1_POINT_2F& contentPoint) const
    = 0;

  virtual void PostCursorEvent(const CursorEvent& ev) = 0;
};

}// namespace OpenKneeboard
