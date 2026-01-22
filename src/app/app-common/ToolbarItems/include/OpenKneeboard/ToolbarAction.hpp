// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/ISelectableToolbarItem.hpp>

#include <OpenKneeboard/utf8.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Foundation.h>

namespace OpenKneeboard {

class ToolbarAction : public virtual ISelectableToolbarItem {
 public:
  virtual ~ToolbarAction();
  std::string_view GetGlyph() const override final;
  std::string_view GetLabel() const override final;

  [[nodiscard]]
  virtual task<void> Execute() = 0;

 protected:
  ToolbarAction() = delete;
  ToolbarAction(std::string glyph, std::string label);

 private:
  std::string mGlyph;
  std::string mLabel;
};

}// namespace OpenKneeboard
