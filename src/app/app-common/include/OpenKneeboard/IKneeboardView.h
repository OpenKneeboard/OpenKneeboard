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
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/inttypes.h>
#include <d2d1.h>

#include <memory>

namespace OpenKneeboard {

enum class UserAction;
class ITabView;
struct CursorEvent;

struct KneeboardViewID final : public UniqueIDBase<KneeboardViewID> {};

class IKneeboardView {
 public:
  IKneeboardView();
  virtual ~IKneeboardView();

  virtual KneeboardViewID GetRuntimeID() const = 0;

  virtual std::shared_ptr<ITabView> GetCurrentTabView() const = 0;
  virtual std::shared_ptr<ITab> GetCurrentTab() const = 0;
  virtual TabIndex GetTabIndex() const = 0;
  virtual std::shared_ptr<ITabView> GetTabViewByID(ITab::RuntimeID) const = 0;
  virtual void SetCurrentTabByIndex(TabIndex) = 0;
  virtual void SetCurrentTabByID(ITab::RuntimeID) = 0;

  virtual void PreviousTab() = 0;
  virtual void NextTab() = 0;

  virtual void NextPage() = 0;
  virtual void PreviousPage() = 0;

  virtual const D2D1_SIZE_U& GetCanvasSize() const = 0;
  virtual const D2D1_RECT_F& GetHeaderRenderRect() const = 0;
  virtual const D2D1_RECT_F& GetContentRenderRect() const = 0;
  /// ContentRenderRect may be scaled; this is the 'real' size.
  virtual const D2D1_SIZE_U& GetContentNativeSize() const = 0;

  Event<TabIndex> evCurrentTabChangedEvent;
  Event<> evNeedsRepaintEvent;
  Event<const CursorEvent&> evCursorEvent;
  Event<> evLayoutChangedEvent;

  virtual bool HaveCursor() const = 0;
  virtual D2D1_POINT_2F GetCursorPoint() const = 0;
  virtual D2D1_POINT_2F GetCursorCanvasPoint() const = 0;
  virtual D2D1_POINT_2F GetCursorCanvasPoint(const D2D1_POINT_2F&) const = 0;

  virtual void PostCursorEvent(const CursorEvent& ev) = 0;
};

}// namespace OpenKneeboard
