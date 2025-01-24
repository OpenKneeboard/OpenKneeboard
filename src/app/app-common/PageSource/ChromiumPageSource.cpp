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

#include <OpenKneeboard/ChromiumPageSource.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/hresult.hpp>

#include <include/cef_client.h>

namespace OpenKneeboard {

class ChromiumPageSource::RenderHandler final : public CefRenderHandler {
 public:
  RenderHandler() = delete;
  RenderHandler(ChromiumPageSource* pageSource) : mPageSource(pageSource) {
    mSize = pageSource->mSettings.mInitialSize;
  }
  ~RenderHandler() {
  }

  void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
    rect = {0, 0, mSize.Width<int>(), mSize.Height<int>()};
  }

  void OnPaint(
    CefRefPtr<CefBrowser>,
    PaintElementType,
    const RectList& dirtyRects,
    const void* buffer,
    int width,
    int height) override {
    fatal(
      "In ChromiumRenderHandler::OnPaint() - should always be using "
      "OnAcceleratedPaint() instead");
  }

  void OnAcceleratedPaint(
    CefRefPtr<CefBrowser>,
    PaintElementType,
    const RectList& dirtyRects,
    const CefAcceleratedPaintInfo& info) {
    auto dxr = mPageSource->mDXResources.get();
    const auto frameCount = mPageSource->mFrameCount + 1;
    const auto frameIndex = frameCount % mPageSource->mFrames.size();
    auto& frame = mPageSource->mFrames.at(frameIndex);

    const PixelSize sourceSize {
      static_cast<uint32_t>(info.extra.visible_rect.width),
      static_cast<uint32_t>(info.extra.visible_rect.height),
    };

    // CEF explicitly bans us from caching the texture for this HANDLE; we need
    // to re-open it every frame
    wil::com_ptr<ID3D11Texture2D> sourceTexture;
    dxr->mD3D11Device->OpenSharedResource1(
      info.shared_texture_handle, IID_PPV_ARGS(sourceTexture.put()));

    if ((!frame.mTexture) || frame.mSize != sourceSize) {
      frame = {};
      OPENKNEEBOARD_ALWAYS_ASSERT(info.format == CEF_COLOR_TYPE_BGRA_8888);
      wil::com_ptr<ID3D11Texture2D> texture;
      D3D11_TEXTURE2D_DESC desc {
        .Width = sourceSize.mWidth,
        .Height = sourceSize.mHeight,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = {1, 0},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
      };
      check_hresult(
        dxr->mD3D11Device->CreateTexture2D(&desc, nullptr, texture.put()));
      wil::com_ptr<ID3D11ShaderResourceView> srv;
      check_hresult(dxr->mD3D11Device->CreateShaderResourceView(
        texture.get(), nullptr, srv.put()));
      frame = {
        .mSize = sourceSize,
        .mTexture = std::move(texture),
        .mShaderResourceView = std::move(srv),
      };
    }

    std::unique_lock lock(*dxr);

    auto ctx = dxr->mD3D11ImmediateContext.get();
    ctx->CopySubresourceRegion(
      frame.mTexture.get(), 0, 0, 0, 0, sourceTexture.get(), 0, nullptr);
    check_hresult(ctx->Signal(mPageSource->mFence.get(), frameCount));
    mPageSource->mFrameCount = frameCount;
  }

 private:
  IMPLEMENT_REFCOUNTING(RenderHandler);

  ChromiumPageSource* mPageSource {nullptr};
  PixelSize mSize {};
};

class ChromiumPageSource::Client final : public CefClient,
                                         public CefLifeSpanHandler,
                                         public CefDisplayHandler {
 public:
  Client() = delete;
  Client(ChromiumPageSource* pageSource) : mPageSource(pageSource) {
    mRenderHandler = {new RenderHandler(pageSource)};
  }
  ~Client() {
  }

  void OnTitleChange(CefRefPtr<CefBrowser>, const CefString& title) override {
    mPageSource->evDocumentTitleChangedEvent.Emit(title);
  }

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
    return this;
  }

  CefRefPtr<CefRenderHandler> GetRenderHandler() override {
    return mRenderHandler;
  }

  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
    return this;
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    mBrowser = browser;
    mBrowserId = mBrowser->GetIdentifier();
  }

  CefRefPtr<CefBrowser> GetBrowser() const {
    return mBrowser;
  }

  std::optional<int> GetBrowserID() const {
    return mBrowserId;
  }

 private:
  IMPLEMENT_REFCOUNTING(Client);

  ChromiumPageSource* mPageSource {nullptr};
  CefRefPtr<CefBrowser> mBrowser;
  CefRefPtr<RenderHandler> mRenderHandler;
  std::optional<int> mBrowserId;
};

ChromiumPageSource::~ChromiumPageSource() = default;

task<std::shared_ptr<ChromiumPageSource>> ChromiumPageSource::Create(
  audited_ptr<DXResources> dxr,
  KneeboardState* kbs,
  Kind kind,
  Settings settings) {
  std::shared_ptr<ChromiumPageSource> ret(
    new ChromiumPageSource(dxr, kbs, kind, settings));
  co_await ret->Init();
  co_return ret;
}

task<void> ChromiumPageSource::Init() {
  mClient = {new Client(this)};

  CefWindowInfo info {};
  info.SetAsWindowless(nullptr);
  info.shared_texture_enabled = true;

  CefBrowserSettings settings;
  settings.windowless_frame_rate = Config::FramesPerSecond;
  if (mSettings.mTransparentBackground) {
    settings.background_color = CefColorSetARGB(0x00, 0x00, 0x00, 0x00);
  }

  CefBrowserHost::CreateBrowser(
    info, mClient, mSettings.mURI, settings, nullptr, nullptr);
  co_return;
}

task<void> ChromiumPageSource::DisposeAsync() noexcept {
  auto disposing = co_await mDisposal.StartOnce();
  co_return;
}

void ChromiumPageSource::PostCursorEvent(
  KneeboardViewID,
  const CursorEvent& ev,
  PageID) {
  if (mFrameCount == 0) {
    return;
  }

  auto host = mClient->GetBrowser()->GetHost();

  const auto epsilon = std::numeric_limits<decltype(ev.mX)>::epsilon();
  if (ev.mX < epsilon || ev.mY < epsilon) {
    if (!mIsHovered) {
      return;
    }
    mIsHovered = false;
    host->SendMouseMoveEvent({}, /* mouseLeave = */ true);
    return;
  }

  mIsHovered = true;
  CefMouseEvent cme;
  cme.x = static_cast<int>(std::lround(ev.mX));
  cme.y = static_cast<int>(std::lround(ev.mY));

  constexpr uint32_t LeftButton = (1 << 0);
  constexpr uint32_t RightButton = (1 << 1);

  // We only pay attention to buttons 1 and 2
  const auto newButtons = ev.mButtons & (LeftButton | RightButton);

  if (newButtons & LeftButton) {
    cme.modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
  }
  if (newButtons & RightButton) {
    cme.modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
  }

  if (mCursorButtons == newButtons) {
    host->SendMouseMoveEvent(cme, /* mouseLeave = */ false);
    return;
  }

  const auto down = newButtons & ~mCursorButtons;
  const auto up = mCursorButtons & ~newButtons;
  using enum cef_mouse_button_type_t;
  if (down & LeftButton) {
    host->SendMouseClickEvent(cme, MBT_LEFT, /* up = */ false, 1);
  }
  if (up & LeftButton) {
    host->SendMouseClickEvent(cme, MBT_LEFT, /* up = */ true, 1);
  }
  if (down & RightButton) {
    host->SendMouseClickEvent(cme, MBT_RIGHT, /* up = */ false, 1);
  }
  if (up & RightButton) {
    host->SendMouseClickEvent(cme, MBT_RIGHT, /* up = */ true, 1);
  }
  mCursorButtons = newButtons;
}

task<void>
ChromiumPageSource::RenderPage(RenderContext rc, PageID id, PixelRect rect) {
  if (id != mPageID || mFrameCount == 0) {
    co_return;
  }

  const auto frameCount = mFrameCount;
  const auto& frame = mFrames.at(frameCount % mFrames.size());

  auto d3d = rc.d3d();
  auto ctx = mDXResources->mD3D11ImmediateContext.get();

  check_hresult(ctx->Wait(mFence.get(), frameCount));
  mSpriteBatch.Begin(d3d.rtv(), rc.GetRenderTarget()->GetDimensions());
  mSpriteBatch.Draw(frame.mShaderResourceView.get(), {0, 0, frame.mSize}, rect);
  mSpriteBatch.End();

  co_return;
}

bool ChromiumPageSource::CanClearUserInput(PageID) const {
  return CanClearUserInput();
}

bool ChromiumPageSource::CanClearUserInput() const {
  // TODO
  return false;
}

void ChromiumPageSource::ClearUserInput(PageID) {
  ClearUserInput();
}

void ChromiumPageSource::ClearUserInput() {
  // TODO
}

PageIndex ChromiumPageSource::GetPageCount() const {
  return (mFrameCount > 0) ? 1 : 0;
}

std::vector<PageID> ChromiumPageSource::GetPageIDs() const {
  if (this->GetPageCount() == 0) {
    return {};
  }
  return {mPageID};
}

std::optional<PreferredSize> ChromiumPageSource::GetPreferredSize(PageID) {
  if (this->GetPageCount() == 0) {
    return std::nullopt;
  }

  const auto& frame = mFrames.at(mFrameCount % mFrames.size());
  return PreferredSize {
    .mPixelSize = frame.mSize,
    .mScalingKind = ScalingKind::Bitmap,
  };
}

ChromiumPageSource::ChromiumPageSource(
  audited_ptr<DXResources> dxr,
  KneeboardState* kbs,
  Kind,
  Settings settings)
  : mDXResources(dxr),
    mKneeboard(kbs),
    mSettings(settings),
    mSpriteBatch(dxr->mD3D11Device.get()) {
  check_hresult(dxr->mD3D11Device->CreateFence(
    0, D3D11_FENCE_FLAG_NONE, IID_PPV_ARGS(mFence.put())));
}

}// namespace OpenKneeboard