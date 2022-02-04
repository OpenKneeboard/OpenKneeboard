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
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/dprint.h>
#include <dxgi1_3.h>
#include <wx/dcbuffer.h>

#include <OpenKneeboard/DXResources.h>
#include "okEvents.h"
#include "scope_guard.h"

using namespace OpenKneeboard;

// TODO: naming consistency
struct okTabCanvas::Impl final {
  std::shared_ptr<Tab> tab;
  uint16_t pageIndex = 0;

  DXResources dxr;
  winrt::com_ptr<IDXGISwapChain1> swapChain;

  std::unique_ptr<D2DErrorRenderer> errorRenderer;

  bool haveCursor = false;
  D2D1_POINT_2F cursorPoint;
  winrt::com_ptr<ID2D1Brush> cursorBrush;
};

okTabCanvas::okTabCanvas(
  wxWindow* parent,
  const DXResources& dxr,
  const std::shared_ptr<Tab>& tab)
  : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize {384, 512}),
    p(new Impl {.tab = tab, .dxr = dxr}) {
  p->errorRenderer = std::make_unique<D2DErrorRenderer>(dxr.mD2DDeviceContext);

  this->Bind(wxEVT_PAINT, &okTabCanvas::OnPaint, this);
  this->Bind(wxEVT_ERASE_BACKGROUND, [](auto) { /* ignore */ });
  this->Bind(wxEVT_SIZE, &okTabCanvas::OnSize, this);

  tab->Bind(okEVT_TAB_FULLY_REPLACED, &okTabCanvas::OnTabFullyReplaced, this);
  tab->Bind(okEVT_TAB_PAGE_MODIFIED, &okTabCanvas::OnTabPageModified, this);
  tab->Bind(okEVT_TAB_PAGE_APPENDED, &okTabCanvas::OnTabPageAppended, this);

  winrt::com_ptr<ID2D1SolidColorBrush> brush;
  winrt::check_hresult(dxr.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), brush.put()));
  p->cursorBrush = brush;
}

okTabCanvas::~okTabCanvas() {
}

void okTabCanvas::OnSize(wxSizeEvent& ev) {
  if (!p->swapChain) {
    return;
  }

  auto size = this->GetClientSize();
  DXGI_SWAP_CHAIN_DESC desc;
  p->swapChain->GetDesc(&desc);

  winrt::com_ptr<ID2D1Image> oldTarget;
  p->dxr.mD2DDeviceContext->SetTarget(nullptr);

  winrt::check_hresult(p->swapChain->ResizeBuffers(
    desc.BufferCount, size.x, size.y, desc.BufferDesc.Format, desc.Flags));
}

void okTabCanvas::OnCursorEvent(const CursorEvent& ev) {
  if (p->pageIndex < p->tab->GetPageCount()) {
    p->tab->OnCursorEvent(ev, p->pageIndex);
  }
  p->haveCursor = ev.TouchState != CursorTouchState::NOT_NEAR_SURFACE;
  p->cursorPoint = {ev.x, ev.y};

  Refresh();
  Update();
}

void okTabCanvas::OnPaint(wxPaintEvent& ev) {
  wxPaintDC dc(this);

  const auto clientSize = GetClientSize();
  if (!p->swapChain) {
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc {
      .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
      .SampleDesc = {1, 0},
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .BufferCount = 2,
      .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
      .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
    };
    p->dxr.mDXGIFactory->CreateSwapChainForHwnd(
      p->dxr.mDXGIDevice.get(),
      this->GetHWND(),
      &swapChainDesc,
      nullptr,
      nullptr,
      p->swapChain.put());
  }

  winrt::com_ptr<IDXGISurface> surface;
  p->swapChain->GetBuffer(0, IID_PPV_ARGS(surface.put()));
  winrt::com_ptr<ID2D1Bitmap1> bitmap;
  auto ctx = p->dxr.mD2DDeviceContext;
  winrt::check_hresult(
    ctx->CreateBitmapFromDxgiSurface(surface.get(), nullptr, bitmap.put()));
  ctx->SetTarget(bitmap.get());

  ctx->BeginDraw();
  const auto cleanup = scope_guard([&]() {
    winrt::check_hresult(ctx->EndDraw());
    winrt::check_hresult(p->swapChain->Present(0, 0));
  });
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());

  const auto count = p->tab->GetPageCount();
  if (count == 0) {
    auto bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    ctx->Clear(D2D1_COLOR_F {
      bg.Red() / 255.0f,
      bg.Green() / 255.0f,
      bg.Blue() / 255.0f,
      bg.Alpha() / 255.0f,
    });

    p->errorRenderer->Render(
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

  p->pageIndex
    = std::clamp(p->pageIndex, 0ui16, static_cast<uint16_t>(count - 1));

  auto contentSize = p->tab->GetPreferredPixelSize(p->pageIndex);

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

  p->tab->RenderPage(p->pageIndex, contentRect);

  if (!p->haveCursor) {
    return;
  }

  auto point = p->cursorPoint;
  point.x *= scale;
  point.y *= scale;

  point.x += (clientSize.x - (contentSize.width * scale)) / 2;
  point.y += (clientSize.y - (contentSize.height * scale)) / 2;

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->DrawEllipse(D2D1::Ellipse(point, 2, 2), p->cursorBrush.get());
}

uint16_t okTabCanvas::GetPageIndex() const {
  return p->pageIndex;
}

void okTabCanvas::SetPageIndex(uint16_t index) {
  const auto count = p->tab->GetPageCount();
  if (count == 0) {
    return;
  }
  p->pageIndex = std::clamp(index, 0ui16, static_cast<uint16_t>(count - 1));

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
  return p->tab;
}

void okTabCanvas::OnTabFullyReplaced(wxCommandEvent&) {
  p->pageIndex = 0;
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
    "Tab appended: {} {} {}", tab->GetTitle(), ev.GetInt(), this->p->pageIndex);
  if (ev.GetInt() != this->p->pageIndex + 1) {
    return;
  }
  this->NextPage();
}

void okTabCanvas::OnPixelsChanged() {
  auto ev = new wxCommandEvent(okEVT_TAB_PIXELS_CHANGED);
  ev->SetEventObject(p->tab.get());
  wxQueueEvent(this, ev);

  this->Refresh();
  this->Update();
}
