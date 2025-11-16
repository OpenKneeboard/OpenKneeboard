// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/IPageSource.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/utf8.hpp>

#include <string>

#include <d2d1_1.h>

namespace OpenKneeboard {

struct Bookmark;

class ITab : public virtual IPageSource {
 public:
  class RuntimeID final : public UniqueIDBase<RuntimeID> {};
  ITab();
  virtual ~ITab();

  [[nodiscard]]
  virtual winrt::guid GetPersistentID() const
    = 0;
  [[nodiscard]]
  virtual std::string GetGlyph() const
    = 0;
  [[nodiscard]]
  virtual std::string GetTitle() const
    = 0;
  virtual void SetTitle(const std::string&) = 0;
  [[nodiscard]]
  virtual RuntimeID GetRuntimeID() const
    = 0;

  [[nodiscard]]
  virtual task<void> Reload()
    = 0;

  [[nodiscard]]
  virtual std::vector<Bookmark> GetBookmarks() const
    = 0;
  virtual void SetBookmarks(const std::vector<Bookmark>&) = 0;

  Event<> evSettingsChangedEvent;
  Event<> evBookmarksChangedEvent;
};

}// namespace OpenKneeboard
