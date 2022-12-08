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
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/TabBase.h>

namespace OpenKneeboard {

static winrt::guid EnsureNonNullGuid(const winrt::guid& guid) {
  if (guid != winrt::guid {}) {
    return guid;
  }

  winrt::guid newGuid;
  winrt::check_hresult(CoCreateGuid(reinterpret_cast<GUID*>(&newGuid)));
  return newGuid;
}

TabBase::TabBase(const winrt::guid& persistentID, utf8_string_view title)
  : mPersistentID(EnsureNonNullGuid(persistentID)), mTitle(title) {
}

ITab::RuntimeID TabBase::GetRuntimeID() const {
  return mRuntimeID;
}

winrt::guid TabBase::GetPersistentID() const {
  return mPersistentID;
}

utf8_string TabBase::GetTitle() const {
  return mTitle;
}

void TabBase::SetTitle(const utf8_string& title) {
  mTitle = title;
  evSettingsChangedEvent.Emit();
}

}// namespace OpenKneeboard
