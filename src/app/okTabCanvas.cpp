/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
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
#include "okTabCanvas.h"

#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <dxgi1_3.h>
#include <wx/dcbuffer.h>

#include "KneeboardState.h"
#include "TabState.h"
#include "okEvents.h"
#include "scope_guard.h"

using namespace OpenKneeboard;

okTabCanvas::okTabCanvas(
  wxWindow* parent,
  const DXResources& dxr,
  const std::shared_ptr<KneeboardState>& kneeboard,
  const std::shared_ptr<TabState>& tab)
  : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize {384, 512}),
    mDXR(dxr),
    mKneeboardState(kneeboard),
    mTabState(tab) {
  // We handle the mouse cursor as if it were a graphics tablet pen
  this->SetCursor(wxCURSOR_BLANK);

  mErrorRenderer = std::make_unique<D2DErrorRenderer>(dxr.mD2DDeviceContext);

  this->Bind(wxEVT_PAINT, &okTabCanvas::OnPaint, this);
  this->Bind(wxEVT_ERASE_BACKGROUND, [](auto) { /* ignore */ });
  this->Bind(wxEVT_SIZE, &okTabCanvas::OnSize, this);

  this->Bind(wxEVT_MOTION, &okTabCanvas::OnMouseMove, this);
  this->Bind(wxEVT_LEAVE_WINDOW, &okTabCanvas::OnMouseLeave, this);

  tab->evNeedsRepaintEvent.AddHandler(this, [=]() { this->EnqueueFrame(); });
  kneeboard->evCursorEvent.AddHandler(
    this, [=](auto) { this->EnqueueFrame(); });

  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), mCursorBrush.put()));
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc {
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .SampleDesc = {1, 0},
    .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
    .BufferCount = 2,
    .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
  };
  winrt::check_hresult(mDXR.mDXGIFactory->CreateSwapChainForHwnd(
    mDXR.mDXGIDevice.get(),
    this->GetHWND(),
    &swapChainDesc,
    nullptr,
    nullptr,
    mSwapChain.put()));

  mFrameTimer.Bind(wxEVT_TIMER, [this](auto&) { this->PaintNow(); });
}

okTabCanvas::~okTabCanvas() {
}

void okTabCanvas::OnSize(wxSizeEvent& ev) {
  auto size = this->GetClientSize();
  DXGI_SWAP_CHAIN_DESC desc;
  mSwapChain->GetDesc(&desc);

  winrt::com_ptr<ID2D1Image> oldTarget;
  mDXR.mD2DDeviceContext->SetTarget(nullptr);

  winrt::check_hresult(mSwapChain->ResizeBuffers(
    desc.BufferCount, size.x, size.y, desc.BufferDesc.Format, desc.Flags));
}

void okTabCanvas::OnPaint(wxPaintEvent& ev) {
  ev.Skip();
  wxPaintDC dc(this);
  PaintNow();
}

void okTabCanvas::PaintNow() {
  const auto clientSize = GetClientSize();

  winrt::com_ptr<IDXGISurface> surface;
  mSwapChain->GetBuffer(0, IID_PPV_ARGS(surface.put()));
  winrt::com_ptr<ID2D1Bitmap1> bitmap;
  auto ctx = mDXR.mD2DDeviceContext;
  winrt::check_hresult(
    ctx->CreateBitmapFromDxgiSurface(surface.get(), nullptr, bitmap.put()));
  ctx->SetTarget(bitmap.get());

  ctx->BeginDraw();
  const auto cleanup = scope_guard([&]() {
    winrt::check_hresult(ctx->EndDraw());
    winrt::check_hresult(mSwapChain->Present(0, 0));
  });
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());

  const auto count = mTabState->GetPageCount();
  if (count == 0) {
    auto bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    ctx->Clear(D2D1_COLOR_F {
      bg.Red() / 255.0f,
      bg.Green() / 255.0f,
      bg.Blue() / 255.0f,
      bg.Alpha() / 255.0f,
    });

    mErrorRenderer->Render(
      _("No Pages").ToStdWstring(),
      {0.0f,
       0.0f,
       float(clientSize.GetWidth()),
       float(clientSize.GetHeight())});
    return;
  }

  auto bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWFRAME);
  ctx->Clear(D2D1_COLOR_F {
    bg.Red() / 255.0f,
    bg.Green() / 255.0f,
    bg.Blue() / 255.0f,
    bg.Alpha() / 255.0f,
  });

  const auto pageMetrics = GetPageMetrics();

  mTabState->GetTab()->RenderPage(
    mTabState->GetPageIndex(), pageMetrics.mRenderRect);

  if (!mKneeboardState->HaveCursor()) {
    return;
  }

  const auto cursorRadius
    = pageMetrics.mRenderSize.height / CursorRadiusDivisor;
  const auto cursorStroke
    = pageMetrics.mRenderSize.height / CursorStrokeDivisor;
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  auto point = mKneeboardState->GetCursorPoint();
  point.x *= pageMetrics.mScale;
  point.y *= pageMetrics.mScale;
  point.x += pageMetrics.mRenderRect.left;
  point.y += pageMetrics.mRenderRect.top;
  ctx->DrawEllipse(
    D2D1::Ellipse(point, cursorRadius, cursorRadius),
    mCursorBrush.get(),
    cursorStroke);
}

okTabCanvas::PageMetrics okTabCanvas::GetPageMetrics() const {
  const auto clientSize = GetClientSize();
  const auto contentNativeSize = mTabState->GetNativeContentSize();

  const auto scaleX
    = static_cast<float>(clientSize.x) / contentNativeSize.width;
  const auto scaleY
    = static_cast<float>(clientSize.y) / contentNativeSize.height;
  const auto scale = std::min(scaleX, scaleY);

  const D2D1_SIZE_F contentRenderSize {
    contentNativeSize.width * scale, contentNativeSize.height * scale};
  const auto padX = (clientSize.x - contentRenderSize.width) / 2;
  const auto padY = (clientSize.y - contentRenderSize.height) / 2;

  const D2D1_RECT_F contentRenderRect {
    .left = padX,
    .top = padY,
    .right = clientSize.x - padX,
    .bottom = clientSize.y - padY,
  };

  return {contentNativeSize, contentRenderRect, contentRenderSize, scale};
}

void okTabCanvas::OnPixelsChanged(wxCommandEvent&) {
  this->Refresh();
}

void okTabCanvas::OnMouseMove(wxMouseEvent& ev) {
  ev.Skip();

  const auto metrics = GetPageMetrics();

  float x = ev.GetX();
  float y = ev.GetY();

  auto positionState
    = (x >= metrics.mRenderRect.left && x <= metrics.mRenderRect.right
       && y >= metrics.mRenderRect.top && y <= metrics.mRenderRect.bottom)
    ? CursorPositionState::IN_CLIENT_RECT
    : CursorPositionState::NOT_IN_CLIENT_RECT;

  x -= metrics.mRenderRect.left;
  y -= metrics.mRenderRect.top;
  x /= metrics.mScale;
  y /= metrics.mScale;

  const bool leftClick = ev.ButtonIsDown(wxMOUSE_BTN_LEFT);
  const bool rightClick = ev.ButtonIsDown(wxMOUSE_BTN_RIGHT);

  mKneeboardState->evCursorEvent({
    .mPositionState = positionState,
    .mTouchState = (leftClick || rightClick)
      ? CursorTouchState::TOUCHING_SURFACE
      : CursorTouchState::NEAR_SURFACE,
    .mX = x,
    .mY = y,
    .mPressure = rightClick ? 0.8f : 0.0f,
    .mButtons = rightClick ? (1ui32 << 1) : 1ui32,
  });
}

void okTabCanvas::OnMouseLeave(wxMouseEvent& ev) {
  ev.Skip();
  mKneeboardState->evCursorEvent({});
}

void okTabCanvas::EnqueueFrame() {
  if (!this->IsShownOnScreen()) {
    return;
  }
  PaintNow();
}
