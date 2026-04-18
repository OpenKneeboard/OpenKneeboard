// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/Pixels.hpp>
#include <OpenKneeboard/PreferredSize.hpp>
#include <OpenKneeboard/ProcessShutdownBlock.hpp>
#include <OpenKneeboard/RenderTarget.hpp>
#include <OpenKneeboard/ThreadGuard.hpp>
#include <OpenKneeboard/UniqueID.hpp>

#include <OpenKneeboard/inttypes.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <d2d1_1.h>

namespace OpenKneeboard {

enum class SuggestedPageAppendAction {
  SwitchToNewPage,
  KeepOnCurrentPage,
};

class IPageSource {
 private:
  ProcessShutdownBlock mBlockShutdownUntilDestroyed;

 public:
  virtual ~IPageSource();

  virtual PageIndex GetPageCount() const = 0;
  virtual std::vector<PageID> GetPageIDs() const = 0;

  virtual std::optional<PreferredSize> GetPreferredSize(PageID) = 0;
  virtual task<void> RenderPage(RenderContext, PageID, PixelRect rect) = 0;

  // Returns a stable, serializable identifier for a page that can survive
  // application restarts. The identifier is an opaque string (the concrete
  // page source chooses its own encoding, typically serialized JSON) and
  // includes any data needed to validate that the underlying content has not
  // changed (e.g. a file hash). Returns nullopt if this page source does not
  // support persistent bookmarks.
  virtual std::optional<std::string> GetPersistentIDForPage(PageID) const = 0;

  // Resolves a previously returned persistent ID back to a live PageID.
  // Returns nullopt if the page is not found or if the underlying content has
  // changed since the identifier was created.
  virtual std::optional<PageID> GetPageIDFromPersistentID(
    std::string_view) const = 0;

  Event<> evNeedsRepaintEvent;
  Event<SuggestedPageAppendAction> evPageAppendedEvent;
  Event<> evContentChangedEvent;
  Event<KneeboardViewID, PageID> evPageChangeRequestedEvent;
  Event<> evAvailableFeaturesChangedEvent;

 protected:
  ThreadGuard mThreadGuard;
};

}// namespace OpenKneeboard
