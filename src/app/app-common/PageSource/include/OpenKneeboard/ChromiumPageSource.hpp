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
#include <OpenKneeboard/D3D11/SpriteBatch.hpp>
#include <OpenKneeboard/IHasDisposeAsync.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/KneeboardViewID.hpp>
#include <OpenKneeboard/Pixels.hpp>
#include <OpenKneeboard/WebPageSourceKind.hpp>
#include <OpenKneeboard/WebPageSourceSettings.hpp>

#include <Windows.h>

#include <wil/com.h>

// TODO: Avoid the need to include this, to avoid poluting with the generic
// macros like `IMPLEMENT_REFCOUNTING`
#include <include/cef_base.h>
#include <include/cef_client.h>

class CefBrowser;

namespace OpenKneeboard {

/// A browser using Chromium Embedded Framework
class ChromiumPageSource final
  : public virtual IPageSourceWithInternalCaching,
    public virtual IPageSourceWithCursorEvents,
    public IHasDisposeAsync,
    public EventReceiver,
    public std::enable_shared_from_this<ChromiumPageSource> {
 public:
  using Kind = WebPageSourceKind;
  using Settings = WebPageSourceSettings;

  ChromiumPageSource() = delete;
  virtual ~ChromiumPageSource();

  static task<std::shared_ptr<ChromiumPageSource>>
  Create(audited_ptr<DXResources>, KneeboardState*, Kind, Settings);
  static task<std::shared_ptr<ChromiumPageSource>>
  Create(audited_ptr<DXResources>, KneeboardState*, std::filesystem::path);

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

  void PostCustomAction(
    KneeboardViewID,
    std::string_view,
    const nlohmann::json& arg) {
    // FIXME: plugins are not yet implemented with Chromium
  }

 private:
  struct JSRequest;
  class Client;
  class RenderHandler;
  struct Frame {
    PixelSize mSize {};
    wil::com_ptr<ID3D11Texture2D> mTexture;
    wil::com_ptr<ID3D11ShaderResourceView> mShaderResourceView;
  };
  using JSAPIResult = std::expected<nlohmann::json, std::string>;

  DisposalState mDisposal;

  audited_ptr<DXResources> mDXResources;
  KneeboardState* mKneeboard {nullptr};
  Settings mSettings {};
  D3D11::SpriteBatch mSpriteBatch;

  bool mIsHovered = false;
  uint32_t mCursorButtons = 0;

  PageID mPageID;

  CefRefPtr<Client> mClient;

  uint64_t mFrameCount = 0;
  wil::com_ptr<ID3D11Fence> mFence;
  std::array<Frame, 3> mFrames;

  ChromiumPageSource(audited_ptr<DXResources>, KneeboardState*, Kind, Settings);
  task<void> Init();
  fire_and_forget OnJSRequest(JSRequest);

  task<JSAPIResult> SetPreferredPixelSize(std::tuple<uint32_t, uint32_t>);
};
}// namespace OpenKneeboard