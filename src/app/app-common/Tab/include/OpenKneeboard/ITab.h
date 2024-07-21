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
#include <OpenKneeboard/IPageSource.h>

#include <OpenKneeboard/utf8.h>

#include <shims/winrt/base.h>

#include <string>

#include <d2d1_1.h>

namespace OpenKneeboard {

struct Bookmark;

class ITab : public virtual IPageSource {
 public:
  class RuntimeID final : public UniqueIDBase<RuntimeID> {};
  ITab();
  virtual ~ITab();

  virtual winrt::guid GetPersistentID() const = 0;
  virtual std::string GetGlyph() const = 0;
  virtual std::string GetTitle() const = 0;
  virtual void SetTitle(const std::string&) = 0;
  virtual RuntimeID GetRuntimeID() const = 0;

  [[nodiscard]]
  virtual winrt::Windows::Foundation::IAsyncAction Reload()
    = 0;

  virtual std::vector<Bookmark> GetBookmarks() const = 0;
  virtual void SetBookmarks(const std::vector<Bookmark>&) = 0;

  Event<> evSettingsChangedEvent;
  Event<> evBookmarksChangedEvent;
};

}// namespace OpenKneeboard
