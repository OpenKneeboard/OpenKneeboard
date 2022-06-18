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
#include <OpenKneeboard/KneeboardViewProxy.h>

namespace OpenKneeboard {

KneeboardViewProxy::KneeboardViewProxy(
  const std::shared_ptr<IKneeboardView>& view) {
  this->SetBackingView(view);
}

KneeboardViewProxy::~KneeboardViewProxy() {
}

void KneeboardViewProxy::SetBackingView(
  const std::shared_ptr<IKneeboardView>& view) {
  const auto oldTab = mView ? this->GetTabIndex() : 0xff;

  for (const auto event: mEventHandlers) {
    RemoveEventListener(event);
  }
  mEventHandlers.clear();

  mView = view;

  AddEventListener(
    view->evCurrentTabChangedEvent, this->evCurrentTabChangedEvent);
  AddEventListener(view->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
  AddEventListener(view->evCursorEvent, this->evCursorEvent);

  const auto newTab = this->GetTabIndex();
  if (oldTab != newTab) {
    this->evCurrentTabChangedEvent.Emit(newTab);
  }
  this->evNeedsRepaintEvent.Emit();
}

uint8_t KneeboardViewProxy::GetTabIndex() const {
  return mView->GetTabIndex();
}

void KneeboardViewProxy::SetCurrentTabByIndex(uint8_t index) {
  mView->SetCurrentTabByIndex(index);
}

void KneeboardViewProxy::SetCurrentTabByID(Tab::RuntimeID id) {
  mView->SetCurrentTabByID(id);
}

void KneeboardViewProxy::PreviousTab() {
  mView->PreviousTab();
}

void KneeboardViewProxy::NextTab() {
  mView->NextTab();
}

std::shared_ptr<TabView> KneeboardViewProxy::GetTabViewByID(
  Tab::RuntimeID id) const {
  return mView->GetTabViewByID(id);
}

std::shared_ptr<Tab> KneeboardViewProxy::GetCurrentTab() const {
  return mView->GetCurrentTab();
}

std::shared_ptr<TabView> KneeboardViewProxy::GetCurrentTabView() const {
  return mView->GetCurrentTabView();
}

void KneeboardViewProxy::NextPage() {
  mView->NextPage();
}

void KneeboardViewProxy::PreviousPage() {
  mView->PreviousPage();
}

const D2D1_SIZE_U& KneeboardViewProxy::GetCanvasSize() const {
  return mView->GetCanvasSize();
}

const D2D1_SIZE_U& KneeboardViewProxy::GetContentNativeSize() const {
  return mView->GetContentNativeSize();
}

const D2D1_RECT_F& KneeboardViewProxy::GetHeaderRenderRect() const {
  return mView->GetHeaderRenderRect();
}

const D2D1_RECT_F& KneeboardViewProxy::GetContentRenderRect() const {
  return mView->GetContentRenderRect();
}

void KneeboardViewProxy::PostCursorEvent(const CursorEvent& ev) {
  mView->PostCursorEvent(ev);
}

bool KneeboardViewProxy::HaveCursor() const {
  return mView->HaveCursor();
}

D2D1_POINT_2F KneeboardViewProxy::GetCursorPoint() const {
  return mView->GetCursorPoint();
}

D2D1_POINT_2F KneeboardViewProxy::GetCursorCanvasPoint() const {
  return this->GetCursorCanvasPoint(this->GetCursorPoint());
}

D2D1_POINT_2F KneeboardViewProxy::GetCursorCanvasPoint(
  const D2D1_POINT_2F& contentPoint) const {
  return mView->GetCursorCanvasPoint(contentPoint);
}

}// namespace OpenKneeboard
