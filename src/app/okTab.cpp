#include "okTab.h"

#include "OpenKneeboard/Tab.h"
#include "okTabCanvas.h"

using namespace OpenKneeboard;

class okTab::Impl final {
 public:
  std::shared_ptr<Tab> Tab;
  okTabCanvas* Canvas;
};

okTab::okTab(wxWindow* parent, const std::shared_ptr<Tab>& tab)
  : wxPanel(parent), p(new Impl {.Tab = tab}) {
  p->Canvas = new okTabCanvas(this, tab);
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

okTab::~okTab() {
}

std::shared_ptr<Tab> okTab::GetTab() const {
  return p->Tab;
}

void okTab::Render(
  const winrt::com_ptr<ID2D1RenderTarget>& target,
  const D2D1_RECT_F& rect) {
  return p->Tab->RenderPage(p->Canvas->GetPageIndex(), target, rect);
}

void okTab::PreviousPage() {
  p->Canvas->PreviousPage();
}

void okTab::NextPage() {
  p->Canvas->NextPage();
}
