#include "okTabCanvas.h"

#include <wx/dcbuffer.h>

#include "OpenKneeboard/Tab.h"

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
  tab->Bind(okEVT_TAB_UPDATED, [this](auto) {
    p->PageIndex = 0;
    this->Refresh();
    wxQueueEvent(this, new wxCommandEvent(okEVT_TAB_UPDATED));
  });
  Bind(wxEVT_PAINT, &okTabCanvas::OnPaint, this);
  Bind(wxEVT_ERASE_BACKGROUND, [](auto) { /* ignore */ });
}

okTabCanvas::~okTabCanvas() {
}

static void
RenderError(const wxSize& clientSize, wxDC& dc, const wxString& message) {
  auto textSize = dc.GetTextExtent(message);
  auto textOrigin = wxPoint {
    (clientSize.GetWidth() - textSize.GetWidth()) / 2,
    (clientSize.GetHeight() - textSize.GetHeight()) / 2};
  dc.SetPen(wxPen(*wxBLACK, 2));
  auto boxSize = wxSize {textSize.GetWidth() + 20, textSize.GetHeight() + 20};
  auto boxOrigin = wxPoint {
    (clientSize.GetWidth() - boxSize.GetWidth()) / 2,
    (clientSize.GetHeight() - boxSize.GetHeight()) / 2};

  dc.SetBrush(*wxGREY_BRUSH);
  dc.DrawRectangle(boxOrigin, boxSize);

  dc.SetBrush(*wxBLACK_BRUSH);
  dc.DrawText(message, textOrigin);
}

void okTabCanvas::OnPaint(wxPaintEvent& ev) {
  const auto count = p->Tab->GetPageCount();
  wxBufferedPaintDC dc(this);
  dc.Clear();

  if (count == 0) {
    RenderError(this->GetClientSize(), dc, _("No Pages"));
    return;
  }
  p->PageIndex = std::clamp(p->PageIndex, 0ui16, count);

  auto image = p->Tab->RenderPage(p->PageIndex);
  if (!image.IsOk()) {
    RenderError(this->GetClientSize(), dc, _("Invalid Page"));
    return;
  }

  const auto imageSize = image.GetSize();
  const auto clientSize = this->GetSize();
  const float xScale = (float)clientSize.GetWidth() / imageSize.GetWidth();
  const float yScale = (float)clientSize.GetHeight() / imageSize.GetHeight();
  const auto scale = std::min(xScale, yScale);
  const auto scaled = image.Scale(
    (int)imageSize.GetWidth() * scale, (int)imageSize.GetHeight() * scale);

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
  p->PageIndex = std::clamp(index, 0ui16, count);
  Refresh();
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
