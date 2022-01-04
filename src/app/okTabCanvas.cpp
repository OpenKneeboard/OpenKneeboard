#include "okTabCanvas.h"

#include <wx/dcbuffer.h>

#include "OpenKneeboard/RenderError.h"
#include "OpenKneeboard/Tab.h"
#include "OpenKneeboard/dprint.h"
#include "okEvents.h"

using namespace OpenKneeboard;

class okTabCanvas::Impl final {
 public:
  std::shared_ptr<Tab> Tab;
  uint16_t PageIndex = 0;
};

okTabCanvas::okTabCanvas(wxWindow* parent, const std::shared_ptr<Tab>& tab)
  : wxPanel(
    parent,
    wxID_ANY,
    wxDefaultPosition,
    wxSize(OPENKNEEBOARD_TEXTURE_WIDTH / 2, OPENKNEEBOARD_TEXTURE_HEIGHT / 2)),
    p(new Impl {.Tab = tab}) {
  Bind(wxEVT_PAINT, &okTabCanvas::OnPaint, this);
  Bind(wxEVT_ERASE_BACKGROUND, [](auto) { /* ignore */ });
  tab->Bind(okEVT_TAB_FULLY_REPLACED, [this](auto&) {
    p->PageIndex = 0;
    this->Refresh();
    wxQueueEvent(this, new wxCommandEvent(okEVT_TAB_NEEDS_REPAINT));
  });
  tab->Bind(okEVT_TAB_PAGE_MODIFIED, [this](wxCommandEvent& ev) {
    this->Refresh();
    wxQueueEvent(this, new wxCommandEvent(okEVT_TAB_NEEDS_REPAINT));
  });
  tab->Bind(okEVT_TAB_PAGE_APPENDED, [this](wxCommandEvent& ev) {
    auto tab = dynamic_cast<Tab*>(ev.GetEventObject());
    if (!tab) {
      return;
    }
    dprintf(
      "Tab appended: {} {} {}",
      tab->GetTitle(),
      ev.GetInt(),
      this->p->PageIndex);
    if (ev.GetInt() != this->p->PageIndex + 1) {
      return;
    }
    this->NextPage();
  });
}

okTabCanvas::~okTabCanvas() {
}

void okTabCanvas::OnPaint(wxPaintEvent& ev) {
  const auto count = p->Tab->GetPageCount();
  wxBufferedPaintDC dc(this);
  dc.Clear();

  if (count == 0) {
    RenderError(this->GetClientSize(), dc, _("No Pages"));
    return;
  }
  p->PageIndex
    = std::clamp(p->PageIndex, 0ui16, static_cast<uint16_t>(count - 1));

  auto image = p->Tab->RenderPage(p->PageIndex);
  if (!image.IsOk()) {
    RenderError(this->GetClientSize(), dc, _("Invalid Page Image"));
    return;
  }

  const auto imageSize = image.GetSize();
  const auto clientSize = this->GetSize();
  const float xScale = (float)clientSize.GetWidth() / imageSize.GetWidth();
  const float yScale = (float)clientSize.GetHeight() / imageSize.GetHeight();
  const auto scale = std::min(xScale, yScale);
  const auto scaled = image.Scale(
    (int)imageSize.GetWidth() * scale,
    (int)imageSize.GetHeight() * scale,
    wxIMAGE_QUALITY_HIGH);

  dc.DrawBitmap(scaled, wxPoint {0, 0});
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
  wxQueueEvent(this, new wxCommandEvent(okEVT_TAB_NEEDS_REPAINT));
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
