// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/IToolbarItem.hpp>

namespace OpenKneeboard {

class ToolbarSeparator final : public IToolbarItem {
 public:
  virtual ~ToolbarSeparator();
};

}// namespace OpenKneeboard
