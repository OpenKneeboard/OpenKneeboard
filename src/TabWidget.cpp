#include "TabWidget.h"

#include "YAVRK/Tab.h"

class TabWidget::Impl final {
 public:
  std::shared_ptr<YAVRK::Tab> Tab;
  uint16_t PageIndex = 0;
};

TabWidget::TabWidget(wxWindow* parent, const std::shared_ptr<YAVRK::Tab>& tab)
  : wxPanel(
    parent,
    wxID_ANY,
    wxDefaultPosition,
    wxSize(YAVRK_TEXTURE_WIDTH / 2, YAVRK_TEXTURE_HEIGHT / 2)),
    p(new Impl {.Tab = tab}) {
}

TabWidget::~TabWidget() {
}

void TabWidget::OnPaint(wxPaintEvent& ev) {
  const auto count = p->Tab->GetPageCount();
  if (count == 0) {
    // TODO: render error thing;
    return;
  }
  p->PageIndex = std::clamp(p->PageIndex, 0ui16, count);

  auto image = p->Tab->RenderPage(p->PageIndex);
  if (!image.IsOk()) {
    return;
  }

  const auto imageSize = image.GetSize();
  const auto widgetSize = this->GetSize();
  const float xScale = (float)widgetSize.GetWidth() / imageSize.GetWidth();
  const float yScale = (float)widgetSize.GetHeight() / imageSize.GetHeight();
  const auto scale = std::min(xScale, yScale);
  const auto scaled = image.Scale(
    (int)imageSize.GetWidth() * scale, (int)imageSize.GetHeight() * scale);

  wxPaintDC dc(this);
  dc.Clear();
  dc.DrawBitmap(scaled, wxPoint {0, 0});
}

void TabWidget::OnEraseBackground(wxEraseEvent& ev) {
  // intentionally empty
}

uint16_t TabWidget::GetPageIndex() const {
  return p->PageIndex;
}

void TabWidget::SetPageIndex(uint16_t index) {
  const auto count = p->Tab->GetPageCount();
  if (count == 0) {
    return;
  }
  p->PageIndex = std::clamp(index, 0ui16, count);
  Refresh();
}

void TabWidget::NextPage() {
  SetPageIndex(GetPageIndex() + 1);
}

void TabWidget::PreviousPage() {
  const auto current = GetPageIndex();
  if (current == 0) {
    return;
  }
  SetPageIndex(current - 1);
}

std::shared_ptr<YAVRK::Tab> TabWidget::GetTab() const {
  return p->Tab;
}

// clang-format off
wxBEGIN_EVENT_TABLE(TabWidget, wxPanel)
  EVT_PAINT(TabWidget::OnPaint)
  EVT_ERASE_BACKGROUND(TabWidget::OnEraseBackground)
wxEND_EVENT_TABLE();
// clang-format on
