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
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/TabViewProxy.h>

namespace OpenKneeboard {

TabViewProxy::TabViewProxy(const std::shared_ptr<ITabView>& impl) {
  this->SetBackingView(impl);
}

TabViewProxy::~TabViewProxy() = default;

void TabViewProxy::SetBackingView(const std::shared_ptr<ITabView>& view) {
  const auto oldPage = mView ? this->GetPageIndex() : ~(0ui16);

  for (const auto event: mEventHandlers) {
    RemoveEventListener(event);
  }
  mView = view;
  mEventHandlers = {
    AddEventListener(view->evCursorEvent, this->evCursorEvent),
    AddEventListener(view->evNeedsRepaintEvent, this->evNeedsRepaintEvent),
    AddEventListener(view->evPageChangedEvent, this->evPageChangedEvent),
    AddEventListener(
      view->evPageChangeRequestedEvent, this->evPageChangeRequestedEvent),
    AddEventListener(
      view->evAvailableFeaturesChangedEvent,
      this->evAvailableFeaturesChangedEvent),
    AddEventListener(view->evTabModeChangedEvent, this->evTabModeChangedEvent),
  };

  if (oldPage != view->GetPageIndex()) {
    this->evPageChangedEvent.Emit();
    this->evNeedsRepaintEvent.Emit();
  }
}

std::shared_ptr<Tab> TabViewProxy::GetRootTab() const {
  return mView->GetRootTab();
}

std::shared_ptr<Tab> TabViewProxy::GetTab() const {
  return mView->GetTab();
}

uint16_t TabViewProxy::GetPageIndex() const {
  return mView->GetPageIndex();
}

void TabViewProxy::PostCursorEvent(const CursorEvent& ev) {
  return mView->PostCursorEvent(ev);
}

uint16_t TabViewProxy::GetPageCount() const {
  return mView->GetPageCount();
}

void TabViewProxy::SetPageIndex(uint16_t page) {
  mView->SetPageIndex(page);
}

void TabViewProxy::NextPage() {
  mView->NextPage();
}

void TabViewProxy::PreviousPage() {
  mView->PreviousPage();
}

D2D1_SIZE_U TabViewProxy::GetNativeContentSize() const {
  return mView->GetNativeContentSize();
}

TabMode TabViewProxy::GetTabMode() const {
  return mView->GetTabMode();
}

bool TabViewProxy::SupportsTabMode(TabMode mode) const {
  return mView->SupportsTabMode(mode);
}

bool TabViewProxy::SetTabMode(TabMode mode) {
  return mView->SetTabMode(mode);
}

}// namespace OpenKneeboard
