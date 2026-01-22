// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/IToolbarItem.hpp>

namespace OpenKneeboard {

struct CurrentPage_t {
  explicit CurrentPage_t() = default;
};
static constexpr CurrentPage_t CurrentPage;

struct AllPages_t {
  explicit AllPages_t() = default;
};
static constexpr AllPages_t AllPages;

struct AllTabs_t {
  explicit AllTabs_t() = default;
};
static constexpr AllTabs_t AllTabs;

class ISelectableToolbarItem : public virtual IToolbarItem {
 public:
  virtual ~ISelectableToolbarItem() = default;

  virtual std::string_view GetGlyph() const = 0;
  virtual std::string_view GetLabel() const = 0;
  virtual bool IsEnabled() const = 0;
};

}// namespace OpenKneeboard
