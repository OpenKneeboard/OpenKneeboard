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
#include "ChromiumPageSource_Client.hpp"

#include "ChromiumPageSource_RenderHandler.hpp"

namespace OpenKneeboard {
namespace {
using JSAPIResult = std::expected<nlohmann::json, std::string>;

template <class... Args>
auto jsapi_error(std::format_string<Args...> fmt, Args&&... args) {
  return std::unexpected {std::format(fmt, std::forward<Args>(args)...)};
}

template <class T>
struct method_reflection;

template <class R, class O, class... Args>
struct method_reflection<R (O::*)(Args...)> {
  using args_tuple = std::tuple<Args...>;
  static constexpr std::size_t args_count = sizeof...(Args);
};

template <auto TMethod>
task<JSAPIResult> JSInvokeSplat(auto self, const nlohmann::json& args) {
  return [&]<std::size_t... I>(std::index_sequence<I...>) {
    return std::invoke(TMethod, self, args.at(I)...);
  }(std::make_index_sequence<
           method_reflection<decltype(TMethod)>::args_count> {});
}

template <auto TMethod>
task<JSAPIResult> JSInvoke(auto self, const nlohmann::json& args) {
  if constexpr (requires { std::invoke(TMethod, self, args); }) {
    co_return co_await std::invoke(TMethod, self, args);
  }

  if (args.is_array()) {
    co_return co_await JSInvokeSplat<TMethod>(self, args);
  }

  co_return jsapi_error("Unable to unpack arguments");
}
}// namespace

ChromiumPageSource::Client::Client(ChromiumPageSource* pageSource)
  : mPageSource(pageSource) {
  mRenderHandler = {new RenderHandler(pageSource)};
}

ChromiumPageSource::Client::~Client() {
}

CefRefPtr<CefLifeSpanHandler> ChromiumPageSource::Client::GetLifeSpanHandler() {
  return this;
}

CefRefPtr<CefRenderHandler> ChromiumPageSource::Client::GetRenderHandler() {
  return mRenderHandler;
}

CefRefPtr<ChromiumPageSource::RenderHandler>
ChromiumPageSource::Client::GetRenderHandlerSubclass() {
  return mRenderHandler;
}

CefRefPtr<CefDisplayHandler> ChromiumPageSource::Client::GetDisplayHandler() {
  return this;
}

bool ChromiumPageSource::Client::OnProcessMessageReceived(
  CefRefPtr<CefBrowser> browser,
  CefRefPtr<CefFrame> frame,
  CefProcessId process,
  CefRefPtr<CefProcessMessage> message) {
  const auto name = message->GetName().ToString();
  if (name.starts_with("okbjs/")) {
    this->OnJSAsyncRequest<&Client::SetPreferredPixelSize>(message);
    return true;
  }

  return CefClient::OnProcessMessageReceived(browser, frame, process, message);
}

template <auto TMethod>
fire_and_forget ChromiumPageSource::Client::OnJSAsyncRequest(
  CefRefPtr<CefProcessMessage> message) {
  const auto callID = message->GetArgumentList()->GetInt(0);
  const auto args = nlohmann::json::parse(
    message->GetArgumentList()->GetString(1).ToString());

  this->SendJSAsyncResult(callID, co_await JSInvoke<TMethod>(this, args));
}

void ChromiumPageSource::Client::SendJSAsyncResult(
  int callID,
  JSAPIResult result) {
  auto data = nlohmann::json::object({});
  if (result.has_value()) {
    data.emplace("result", result->is_null() ? "ok" : result.value());
  } else {
    data.emplace("error", result.error());
    dprint.Warning("JS API error: {}", result.error());
  }

  auto message = CefProcessMessage::Create("okb/asyncResult");
  auto args = message->GetArgumentList();
  args->SetInt(0, callID);
  args->SetString(1, data.dump());
  mBrowser->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
}

void ChromiumPageSource::Client::OnTitleChange(
  CefRefPtr<CefBrowser>,
  const CefString& title) {
  mPageSource->evDocumentTitleChangedEvent.Emit(title);
}

void ChromiumPageSource::Client::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  mBrowser = browser;
  mBrowserId = mBrowser->GetIdentifier();
}

CefRefPtr<CefBrowser> ChromiumPageSource::Client::GetBrowser() const {
  return mBrowser;
}

std::optional<int> ChromiumPageSource::Client::GetBrowserID() const {
  return mBrowserId;
}

void ChromiumPageSource::Client::PostCursorEvent(const CursorEvent& ev) {
  auto host = this->GetBrowser()->GetHost();

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

task<JSAPIResult> ChromiumPageSource::Client::SetPreferredPixelSize(
  uint32_t width,
  uint32_t height) {
  if (width < 1 || height < 1) {
    co_return jsapi_error("Requested 0px area, ignoring");
  }

  PixelSize size {width, height};
  if (
    width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION
    || height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) {
    dprint.Warning(
      "Web page requested resize to {}x{}, which is outside of D3D11 limits",
      width,
      height);
    size = size.ScaledToFit(
      {
        D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION,
        D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION,
      },
      Geometry2D::ScaleToFitMode::ShrinkOnly);
    if (size.mWidth < 1 || size.mHeight < 1) {
      co_return jsapi_error(
        "Requested size scales down to < 1px in at least 1 dimension");
    }
    dprint("Shrunk to fit: {}x{}", size.mWidth, size.mHeight);
  }

  this->GetRenderHandlerSubclass()->SetSize(size);
  this->GetBrowser()->GetHost()->WasResized();

  co_return nlohmann::json {
    {"result", "resized"},
    {
      "details",
      {
        {"width", size.mWidth},
        {"height", size.mHeight},
      },
    },
  };
}

}// namespace OpenKneeboard