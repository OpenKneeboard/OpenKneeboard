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

#include "okEvents.h"
#include "scope_guard.h"

using namespace OpenKneeboard;

struct okTabCanvas::Impl final {
  std::shared_ptr<Tab> mTab;
  uint16_t mPageIndex = 0;

  DXResources mDXR;
  winrt::com_ptr<IDXGISwapChain1> mSwapChain;

  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;

  bool mHaveCursor = false;
  D2D1_POINT_2F mCursorPoint;
  winrt::com_ptr<ID2D1Brush> mCursorBrush;
};

okTabCanvas::okTabCanvas(
  wxWindow* parent,
  const DXResources& dxr,
  const std::shared_ptr<Tab>& tab)
  : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize {384, 512}),
    p(new Impl {.mTab = tab, .mDXR = dxr}) {
  // We handle the mouse cursor as if it were a graphics tablet pen
  this->SetCursor(wxCURSOR_BLANK);

  p->mErrorRenderer = std::make_unique<D2DErrorRenderer>(dxr.mD2DDeviceContext);

  this->Bind(wxEVT_PAINT, &okTabCanvas::OnPaint, this);
  this->Bind(wxEVT_ERASE_BACKGROUND, [](auto) { /* ignore */ });
  this->Bind(wxEVT_SIZE, &okTabCanvas::OnSize, this);

  this->Bind(wxEVT_MOTION, &okTabCanvas::OnMouseMove, this);
  this->Bind(wxEVT_LEAVE_WINDOW, &okTabCanvas::OnMouseLeave, this);

  tab->Bind(okEVT_TAB_FULLY_REPLACED, &okTabCanvas::OnTabFullyReplaced, this);
  tab->Bind(okEVT_TAB_PAGE_MODIFIED, &okTabCanvas::OnTabPageModified, this);
  tab->Bind(okEVT_TAB_PAGE_APPENDED, &okTabCanvas::OnTabPageAppended, this);

  winrt::com_ptr<ID2D1SolidColorBrush> brush;
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), brush.put()));
  p->mCursorBrush = brush;
}

okTabCanvas::~okTabCanvas() {
}

void okTabCanvas::OnSize(wxSizeEvent& ev) {
  if (!p->mSwapChain) {
    return;
  }

  auto size = this->GetClientSize();
  DXGI_SWAP_CHAIN_DESC desc;
  p->mSwapChain->GetDesc(&desc);

  winrt::com_ptr<ID2D1Image> oldTarget;
  p->mDXR.mD2DDeviceContext->SetTarget(nullptr);

  winrt::check_hresult(p->mSwapChain->ResizeBuffers(
    desc.BufferCount, size.x, size.y, desc.BufferDesc.Format, desc.Flags));
}

void okTabCanvas::OnCursorEvent(const CursorEvent& ev) {
  if (p->mPageIndex < p->mTab->GetPageCount()) {
    p->mTab->OnCursorEvent(ev, p->mPageIndex);
  }
  p->mHaveCursor = ev.mTouchState != CursorTouchState::NOT_NEAR_SURFACE;
  p->mCursorPoint = {ev.mX, ev.mY};

  Refresh();
  Update();
}

void okTabCanvas::OnPaint(wxPaintEvent& ev) {
  wxPaintDC dc(this);

  const auto clientSize = GetClientSize();
  if (!p->mSwapChain) {
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc {
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
      .SampleDesc = {1, 0},
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .BufferCount = 2,
      .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
      .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
    };
    p->mDXR.mDXGIFactory->CreateSwapChainForHwnd(
      p->mDXR.mDXGIDevice.get(),
      this->GetHWND(),
      &swapChainDesc,
      nullptr,
      nullptr,
      p->mSwapChain.put());
  }

  winrt::com_ptr<IDXGISurface> surface;
  p->mSwapChain->GetBuffer(0, IID_PPV_ARGS(surface.put()));
  winrt::com_ptr<ID2D1Bitmap1> bitmap;
  auto ctx = p->mDXR.mD2DDeviceContext;
  winrt::check_hresult(
    ctx->CreateBitmapFromDxgiSurface(surface.get(), nullptr, bitmap.put()));
  ctx->SetTarget(bitmap.get());

  ctx->BeginDraw();
  const auto cleanup = scope_guard([&]() {
    winrt::check_hresult(ctx->EndDraw());
    winrt::check_hresult(p->mSwapChain->Present(0, 0));
  });
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());

  const auto count = p->mTab->GetPageCount();
  if (count == 0) {
    auto bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    ctx->Clear(D2D1_COLOR_F {
      bg.Red() / 255.0f,
      bg.Green() / 255.0f,
      bg.Blue() / 255.0f,
      bg.Alpha() / 255.0f,
    });

    p->mErrorRenderer->Render(
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

  p->mPageIndex
    = std::clamp(p->mPageIndex, 0ui16, static_cast<uint16_t>(count - 1));

  const auto pageMetrics = GetPageMetrics();

  p->mTab->RenderPage(p->mPageIndex, pageMetrics.mRect);

  if (!p->mHaveCursor) {
    return;
  }

  const auto scale = pageMetrics.mScale;
  const auto pageSize = pageMetrics.mSize;

  auto point = p->mCursorPoint;
  point.x *= scale;
  point.y *= scale;

  point.x += (clientSize.x - pageSize.width) / 2;
  point.y += (clientSize.y - pageSize.height) / 2;

  const auto cursorRadius = clientSize.y / CursorRadiusDivisor;
  const auto cursorStroke = clientSize.y / CursorStrokeDivisor;
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->DrawEllipse(
    D2D1::Ellipse(point, cursorRadius, cursorRadius),
    p->mCursorBrush.get(),
    cursorStroke);
}

okTabCanvas::PageMetrics okTabCanvas::GetPageMetrics() const {
  if (p->mPageIndex >= p->mTab->GetPageCount()) {
    return {};
  }
  const auto clientSize = GetClientSize();
  const auto preferredSize = p->mTab->GetPreferredPixelSize(p->mPageIndex);

  const auto scaleX = static_cast<float>(clientSize.x) / preferredSize.width;
  const auto scaleY = static_cast<float>(clientSize.y) / preferredSize.height;
  const auto scale = std::min(scaleX, scaleY);

  const D2D1_SIZE_F pageSize {
    preferredSize.width * scale, preferredSize.height * scale};
  const auto padX = (clientSize.x - pageSize.width) / 2;
  const auto padY = (clientSize.y - pageSize.height) / 2;
  const D2D1_RECT_F pageRect {
    .left = padX,
    .top = padY,
    .right = clientSize.x - padX,
    .bottom = clientSize.y - padY,
  };

  return {pageRect, pageSize, scale};
}

uint16_t okTabCanvas::GetPageIndex() const {
  return p->mPageIndex;
}

void okTabCanvas::SetPageIndex(uint16_t index) {
  const auto count = p->mTab->GetPageCount();
  if (count == 0) {
    return;
  }
  p->mPageIndex = std::clamp(index, 0ui16, static_cast<uint16_t>(count - 1));

  this->OnPixelsChanged();
}

void okTabCanvas::NextPage() {
  SetPageIndex(GetPageIndex() + 1);
}

void okTabCanvas::PreviousPage() {
  const auto current = GetPageIndex();
  if (current == 0) {
    return;
  }
  SetPageIndex(current - 1);
}

std::shared_ptr<Tab> okTabCanvas::GetTab() const {
  return p->mTab;
}

void okTabCanvas::OnTabFullyReplaced(wxCommandEvent&) {
  p->mPageIndex = 0;
  this->OnPixelsChanged();
}

void okTabCanvas::OnTabPageModified(wxCommandEvent&) {
  this->OnPixelsChanged();
}

void okTabCanvas::OnTabPageAppended(wxCommandEvent& ev) {
  auto tab = dynamic_cast<Tab*>(ev.GetEventObject());
  if (!tab) {
    return;
  }
  dprintf(
    "Tab appended: {} {} {}",
    tab->GetTitle(),
    ev.GetInt(),
    this->p->mPageIndex);
  if (ev.GetInt() != this->p->mPageIndex + 1) {
    return;
  }
  this->NextPage();
}

void okTabCanvas::OnPixelsChanged() {
  auto ev = new wxCommandEvent(okEVT_TAB_PIXELS_CHANGED);
  ev->SetEventObject(p->mTab.get());
  wxQueueEvent(this, ev);

  this->Refresh();
  this->Update();
}

void okTabCanvas::OnMouseMove(wxMouseEvent& ev) {
  ev.StopPropagation();

  if (GetPageIndex() >= p->mTab->GetPageCount()) {
    return;
  }

  const auto metrics = GetPageMetrics();

  auto x = ev.GetX() / metrics.mScale;
  auto y = ev.GetY() / metrics.mScale;

  const bool leftClick = ev.ButtonIsDown(wxMOUSE_BTN_LEFT);
  const bool rightClick = ev.ButtonIsDown(wxMOUSE_BTN_RIGHT);

  this->OnCursorEvent({
    .mPositionState = CursorPositionState::IN_CLIENT_RECT,
    .mTouchState
    = (leftClick || rightClick)
      ? CursorTouchState::TOUCHING_SURFACE
      : CursorTouchState::NEAR_SURFACE,
    .mX = x,
    .mY = y,
    .mPressure = rightClick ? 0.8f : 0.0f,
    .mButtons = rightClick ? (1ui32 << 1) : 1ui32,
  });
}

void okTabCanvas::OnMouseLeave(wxMouseEvent& ev) {
  this->OnCursorEvent({
    .mPositionState = CursorPositionState::NOT_IN_CLIENT_RECT,
    .mTouchState = CursorTouchState::NOT_NEAR_SURFACE,
  });
}
