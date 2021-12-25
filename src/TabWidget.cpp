#include "TabWidget.h"

#include "TabCanvasWidget.h"
#include "YAVRK/Tab.h"

class TabWidget::Impl final {
 public:
  std::shared_ptr<YAVRK::Tab> Tab;
  TabCanvasWidget* Canvas;
};

TabWidget::TabWidget(wxWindow* parent, const std::shared_ptr<YAVRK::Tab>& tab)
  : wxPanel(parent), p(new Impl {.Tab = tab}) {
  p->Canvas = new TabCanvasWidget(this, tab);
  auto canvas = p->Canvas;

  auto sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(p->Canvas);

  auto buttonBox = new wxPanel(this);
  auto firstPage = new wxButton(buttonBox, wxID_ANY, _("F&irst Page"));
  auto previousPage = new wxButton(buttonBox, wxID_ANY, _("&Previous Page"));
  auto nextPage = new wxButton(buttonBox, wxID_ANY, _("&Next Page"));
  firstPage->Bind(wxEVT_BUTTON, [=](auto) { canvas->SetPageIndex(0); });
  previousPage->Bind(wxEVT_BUTTON, [=](auto) { canvas->PreviousPage(); });
  nextPage->Bind(wxEVT_BUTTON, [=](auto) { canvas->NextPage(); });

  auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  buttonSizer->Add(firstPage);
  buttonSizer->AddStretchSpacer();
  buttonSizer->Add(previousPage);
  buttonSizer->Add(nextPage);
  buttonBox->SetSizer(buttonSizer);
  sizer->Add(buttonBox, 0, wxEXPAND);

  this->SetSizerAndFit(sizer);
}

TabWidget::~TabWidget() {
}

std::shared_ptr<YAVRK::Tab> TabWidget::GetTab() const {
  return p->Tab;
}

wxImage TabWidget::GetImage() {
  return p->Tab->RenderPage(p->Canvas->GetPageIndex());
}

// clang-format off
wxBEGIN_EVENT_TABLE(TabWidget, wxPanel)
wxEND_EVENT_TABLE();
// clang-format on
