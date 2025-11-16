// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once
#include "IPageSource.hpp"
namespace OpenKneeboard {

/** Marker interface to disable any implicit caching in wrappers. */
class IPageSourceWithInternalCaching : public virtual IPageSource {};
}// namespace OpenKneeboard