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

#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/dprint.h>
#include <wx/dcbuffer.h>

#include "okEvents.h"

using namespace OpenKneeboard;

class okTabCanvas::Impl final {
 public:
  std::shared_ptr<Tab> tab;
  uint16_t pageIndex = 0;

  winrt::com_ptr<ID2D1Factory> d2d;
  winrt::com_ptr<ID2D1HwndRenderTarget> rt;

  std::unique_ptr<D2DErrorRenderer> errorRenderer;
};

okTabCanvas::okTabCanvas(wxWindow* parent, const std::shared_ptr<Tab>& tab)
  : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize {384, 512}),
    p(new Impl {.tab = tab}) {
  D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, p->d2d.put());
  p->errorRenderer = std::make_unique<D2DErrorRenderer>(p->d2d);

  this->Bind(wxEVT_PAINT, &okTabCanvas::OnPaint, this);
  this->Bind(wxEVT_ERASE_BACKGROUND, [](auto) { /* ignore */ });
  this->Bind(wxEVT_SIZE, &okTabCanvas::OnSize, this);

  tab->Bind(okEVT_TAB_FULLY_REPLACED, &okTabCanvas::OnTabFullyReplaced, this);
  tab->Bind(okEVT_TAB_PAGE_MODIFIED, &okTabCanvas::OnTabPageModified, this);
  tab->Bind(okEVT_TAB_PAGE_APPENDED, &okTabCanvas::OnTabPageAppended, this);
}

okTabCanvas::~okTabCanvas() {
}

void okTabCanvas::OnSize(wxSizeEvent& ev) {
  if (!p->rt) {
    return;
  }

  auto size = this->GetClientSize();
  p->rt->Resize(
    {static_cast<UINT>(size.GetWidth()), static_cast<UINT>(size.GetHeight())});
}

void okTabCanvas::OnPaint(wxPaintEvent& ev) {
  wxPaintDC dc(this);

  const auto clientSize = GetClientSize();
  if (!p->rt) {
    auto rtp = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1_PIXEL_FORMAT {
        DXGI_FORMAT_B8G8R8A8_UNORM,
        D2D1_ALPHA_MODE_PREMULTIPLIED,
      });
    rtp.dpiX = 0;
    rtp.dpiY = 0;

    auto hwndRtp = D2D1::HwndRenderTargetProperties(
      this->GetHWND(),
      {static_cast<UINT32>(clientSize.GetWidth()),
       static_cast<UINT32>(clientSize.GetHeight())});

    p->d2d->CreateHwndRenderTarget(&rtp, &hwndRtp, p->rt.put());
    p->rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    p->errorRenderer->SetRenderTarget(p->rt);
  }

  p->rt->BeginDraw();
  wxON_BLOCK_EXIT0([this]() { this->p->rt->EndDraw(); });
  p->rt->SetTransform(D2D1::Matrix3x2F::Identity());

  const auto count = p->tab->GetPageCount();
  if (count == 0) {
    auto bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    p->rt->Clear(D2D1_COLOR_F {
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
  p->rt->Clear(D2D1_COLOR_F {
    bg.Red() / 255.0f,
    bg.Green() / 255.0f,
    bg.Blue() / 255.0f,
    bg.Alpha() / 255.0f,
  });

  p->pageIndex
    = std::clamp(p->pageIndex, 0ui16, static_cast<uint16_t>(count - 1));

  p->tab->RenderPage(
    p->pageIndex,
    p->rt,
    {0.0f, 0.0f, float(clientSize.GetWidth()), float(clientSize.GetHeight())});
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
