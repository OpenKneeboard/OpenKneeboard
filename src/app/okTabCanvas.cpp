#include "okTabCanvas.h"

#include <wx/dcbuffer.h>

#include "OpenKneeboard/D2DErrorRenderer.h"
#include "OpenKneeboard/Tab.h"
#include "OpenKneeboard/dprint.h"
#include "okEvents.h"

using namespace OpenKneeboard;

class okTabCanvas::Impl final {
 public:
  std::shared_ptr<Tab> Tab;
  uint16_t PageIndex = 0;

  winrt::com_ptr<ID2D1Factory> D2d;
  winrt::com_ptr<ID2D1HwndRenderTarget> RT;

  std::unique_ptr<D2DErrorRenderer> ErrorRenderer;
};

okTabCanvas::okTabCanvas(wxWindow* parent, const std::shared_ptr<Tab>& tab)
  : wxPanel(
    parent,
    wxID_ANY,
    wxDefaultPosition,
    wxSize(OPENKNEEBOARD_TEXTURE_WIDTH / 2, OPENKNEEBOARD_TEXTURE_HEIGHT / 2)),
    p(new Impl {.Tab = tab}) {
  D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, p->D2d.put());
  p->ErrorRenderer = std::make_unique<D2DErrorRenderer>(p->D2d);

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
  if (!p->RT) {
    return;
  }

  auto size = this->GetClientSize();
  p->RT->Resize(
    {static_cast<UINT>(size.GetWidth()), static_cast<UINT>(size.GetHeight())});
}

void okTabCanvas::OnPaint(wxPaintEvent& ev) {
  wxPaintDC dc(this);

  const auto clientSize = GetClientSize();
  if (!p->RT) {
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

    p->D2d->CreateHwndRenderTarget(&rtp, &hwndRtp, p->RT.put());

    p->ErrorRenderer->SetRenderTarget(p->RT);
  }

  p->RT->BeginDraw();
  wxON_BLOCK_EXIT0([this]() { this->p->RT->EndDraw(); });
  p->RT->SetTransform(D2D1::Matrix3x2F::Identity());

  const auto count = p->Tab->GetPageCount();
  if (count == 0) {
    auto bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    p->RT->Clear(D2D1_COLOR_F {
      bg.Red() / 255.0f,
      bg.Green() / 255.0f,
      bg.Blue() / 255.0f,
      bg.Alpha() / 255.0f,
    });
    
    p->ErrorRenderer->Render(
      _("No Pages").ToStdWstring(),
      {0.0f,
       0.0f,
       float(clientSize.GetWidth()),
       float(clientSize.GetHeight())});
    return;
  }

  auto bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWFRAME);
  p->RT->Clear(D2D1_COLOR_F {
    bg.Red() / 255.0f,
    bg.Green() / 255.0f,
    bg.Blue() / 255.0f,
    bg.Alpha() / 255.0f,
  });

  p->PageIndex
    = std::clamp(p->PageIndex, 0ui16, static_cast<uint16_t>(count - 1));

  p->Tab->RenderPage(
    p->PageIndex,
    p->RT,
    {0.0f, 0.0f, float(clientSize.GetWidth()), float(clientSize.GetHeight())});
}

uint16_t okTabCanvas::GetPageIndex() const {
  return p->PageIndex;
}

void okTabCanvas::SetPageIndex(uint16_t index) {
  const auto count = p->Tab->GetPageCount();
  if (count == 0) {
    return;
  }
  p->PageIndex = std::clamp(index, 0ui16, static_cast<uint16_t>(count - 1));

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
  return p->Tab;
}

void okTabCanvas::OnTabFullyReplaced(wxCommandEvent&) {
  p->PageIndex = 0;
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
    "Tab appended: {} {} {}", tab->GetTitle(), ev.GetInt(), this->p->PageIndex);
  if (ev.GetInt() != this->p->PageIndex + 1) {
    return;
  }
  this->NextPage();
}

void okTabCanvas::OnPixelsChanged() {
  auto ev = new wxCommandEvent(okEVT_TAB_PIXELS_CHANGED);
  ev->SetEventObject(p->Tab.get());
  wxQueueEvent(this, ev);

  this->Refresh();
  this->Update();
}
