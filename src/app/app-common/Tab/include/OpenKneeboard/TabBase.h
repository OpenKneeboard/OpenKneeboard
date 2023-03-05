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

#include "ITab.h"

#include <OpenKneeboard/Bookmark.h>

namespace OpenKneeboard {

class TabBase : public virtual ITab, public virtual EventReceiver {
 public:
  ~TabBase();

  virtual RuntimeID GetRuntimeID() const override final;
  virtual winrt::guid GetPersistentID() const override final;
  virtual std::string GetTitle() const override final;
  virtual void SetTitle(const std::string&) override final;

  virtual std::vector<Bookmark> GetBookmarks() const override final;
  virtual void SetBookmarks(const std::vector<Bookmark>&) override final;

 protected:
  TabBase(const winrt::guid& persistentID, std::string_view title);

 private:
  const winrt::guid mPersistentID;
  const RuntimeID mRuntimeID;
  std::string mTitle;
  std::vector<Bookmark> mBookmarks;

  void OnContentChanged();
};

}// namespace OpenKneeboard
