// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/ITab.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <atomic>

namespace OpenKneeboard {

static std::atomic_size_t gTabCount {0};
static const scope_exit gCheckForTabLeaks([]() {
  const auto count = gTabCount.load();
  if (count > 0) {
    dprint("Leaking {} tabs", count);
    OPENKNEEBOARD_BREAK;
  }
});

ITab::ITab() {
  gTabCount.fetch_add(1);
}

ITab::~ITab() {
  gTabCount.fetch_sub(1);
}

}// namespace OpenKneeboard
