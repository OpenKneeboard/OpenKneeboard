/*
 * OpenKneeboard
 *
 * Copyright (C) 2025 Fred Emmott <fred@fredemmott.com>
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

#include "IPageSourceWithCursorEvents.hpp"

#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/IHasDisposeAsync.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/KneeboardViewID.hpp>

// TODO: Avoid the need to include this, to avoid poluting with the generic
// macros like `IMPLEMENT_REFCOUNTING`
#include <include/cef_base.h>
#include <include/cef_client.h>

namespace OpenKneeboard {

/// A browser using Chromium Embedded Framework
class ChromiumPageSource final
  : public virtual IPageSourceWithInternalCaching,
    public virtual IPageSourceWithCursorEvents,
    public IHasDisposeAsync,
    public EventReceiver,
    public std::enable_shared_from_this<ChromiumPageSource> {
 public:
  ChromiumPageSource() = delete;
  virtual ~ChromiumPageSource();

  static task<std::shared_ptr<ChromiumPageSource>> Create(
    audited_ptr<DXResources>,
    KneeboardState*);

  task<void> DisposeAsync() noexcept override;

  void PostCursorEvent(KneeboardViewID, const CursorEvent&, PageID) override;
  task<void> RenderPage(RenderContext, PageID, PixelRect rect) override;

  bool CanClearUserInput(PageID) const override;
  bool CanClearUserInput() const override;
  void ClearUserInput(PageID) override;
  void ClearUserInput() override;

  PageIndex GetPageCount() const override;
  std::vector<PageID> GetPageIDs() const override;
  std::optional<PreferredSize> GetPreferredSize(PageID) override;

  Event<std::string> evDocumentTitleChangedEvent;

 private:
  class Client;

  winrt::apartment_context mUIThread;
  DisposalState mDisposal;

  PageID mPageID;

  CefRefPtr<Client> mClient;

  ChromiumPageSource(const audited_ptr<DXResources>&, KneeboardState*);
  task<void> Init();

  audited_ptr<DXResources> mDXResources;
  KneeboardState* mKneeboard {nullptr};
};
}// namespace OpenKneeboard