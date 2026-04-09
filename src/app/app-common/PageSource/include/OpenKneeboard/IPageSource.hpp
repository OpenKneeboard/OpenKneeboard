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

#include <shims/nlohmann/json.hpp>

#include <cstdint>
#include <optional>

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
  // application restarts. The identifier includes any data needed to validate
  // that the underlying content has not changed (e.g. a file hash).
  // Returns nullopt if this page source does not support persistent bookmarks.
  virtual std::optional<nlohmann::json> GetPersistentIDForPage(PageID) const;

  // Resolves a previously returned persistent ID back to a live PageID.
  // Returns nullopt if the page is not found or if the underlying content has
  // changed since the identifier was created.
  virtual std::optional<PageID> GetPageIDFromPersistentID(
    const nlohmann::json&) const;

  Event<> evNeedsRepaintEvent;
  Event<SuggestedPageAppendAction> evPageAppendedEvent;
  Event<> evContentChangedEvent;
  Event<KneeboardViewID, PageID> evPageChangeRequestedEvent;
  Event<> evAvailableFeaturesChangedEvent;

 protected:
  ThreadGuard mThreadGuard;
};

}// namespace OpenKneeboard
