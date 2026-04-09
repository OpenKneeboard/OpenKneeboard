// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/IPageSource.hpp>

namespace OpenKneeboard {

IPageSource::~IPageSource() = default;

std::optional<nlohmann::json> IPageSource::GetPersistentIDForPage(PageID) const {
  return std::nullopt;
}

std::optional<PageID> IPageSource::GetPageIDFromPersistentID(
  const nlohmann::json&) const {
  return std::nullopt;
}

}
