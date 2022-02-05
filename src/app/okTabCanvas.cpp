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
  p->mErrorRenderer = std::make_unique<D2DErrorRenderer>(dxr.mD2DDeviceContext);

  this->Bind(wxEVT_PAINT, &okTabCanvas::OnPaint, this);
  this->Bind(wxEVT_ERASE_BACKGROUND, [](auto) { /* ignore */ });
  this->Bind(wxEVT_SIZE, &okTabCanvas::OnSize, this);

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
  p->mHaveCursor = ev.TouchState != CursorTouchState::NOT_NEAR_SURFACE;
  p->mCursorPoint = {ev.x, ev.y};

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

  auto contentSize = p->mTab->GetPreferredPixelSize(p->mPageIndex);

  const auto scaleX = static_cast<float>(clientSize.x) / contentSize.width;
  const auto scaleY = static_cast<float>(clientSize.y) / contentSize.height;
  const auto scale = std::min(scaleX, scaleY);

  const D2D1_SIZE_F scaledContentSize
    = {contentSize.width * scale, contentSize.height * scale};
  const auto padX = (clientSize.x - scaledContentSize.width) / 2;
  const auto padY = (clientSize.y - scaledContentSize.height) / 2;
  const D2D1_RECT_F contentRect = {
    .left = padX,
    .top = padY,
    .right = clientSize.x - padX,
    .bottom = clientSize.y - padY,
  };

  p->mTab->RenderPage(p->mPageIndex, contentRect);

  if (!p->mHaveCursor) {
    return;
  }

  auto point = p->mCursorPoint;
  point.x *= scale;
  point.y *= scale;

  point.x += (clientSize.x - (contentSize.width * scale)) / 2;
  point.y += (clientSize.y - (contentSize.height * scale)) / 2;

  const auto cursorRadius = clientSize.y / CursorRadiusDivisor;
  const auto cursorStroke = clientSize.y / CursorStrokeDivisor;
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->DrawEllipse(
    D2D1::Ellipse(point, cursorRadius, cursorRadius),
    p->mCursorBrush.get(),
    cursorStroke);
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
