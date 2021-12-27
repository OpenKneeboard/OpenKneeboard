#include "okTab.h"

#include "OpenKneeboard/Tab.h"
#include "okTabCanvas.h"

wxDEFINE_EVENT(okEVT_PAGE_CHANGED, wxCommandEvent);

class okTab::Impl final {
 public:
  std::shared_ptr<OpenKneeboard::Tab> Tab;
  okTabCanvas* Canvas;
};

okTab::okTab(
  wxWindow* parent,
  const std::shared_ptr<OpenKneeboard::Tab>& tab)
  : wxPanel(parent), p(new Impl {.Tab = tab}) {
  p->Canvas = new okTabCanvas(this, tab);
  auto canvas = p->Canvas;
  canvas->Bind(okEVT_TAB_UPDATED, [this](auto) { wxQueueEvent(this, new wxCommandEvent(okEVT_TAB_UPDATED)); });

  auto sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(p->Canvas);

  auto buttonBox = new wxPanel(this);
  auto firstPage = new wxButton(buttonBox, wxID_ANY, _("F&irst Page"));
  auto previousPage = new wxButton(buttonBox, wxID_ANY, _("&Previous Page"));
  auto nextPage = new wxButton(buttonBox, wxID_ANY, _("&Next Page"));
  firstPage->Bind(wxEVT_BUTTON, [=](auto) {
    canvas->SetPageIndex(0);
    this->EmitPageChanged();
  });
  previousPage->Bind(wxEVT_BUTTON, [=](auto) {
    canvas->PreviousPage();
    this->EmitPageChanged();
  });
  nextPage->Bind(wxEVT_BUTTON, [=](auto) {
    canvas->NextPage();
    this->EmitPageChanged();
  });

  auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  buttonSizer->Add(firstPage);
  buttonSizer->AddStretchSpacer();
  buttonSizer->Add(previousPage);
  buttonSizer->Add(nextPage);
  buttonBox->SetSizer(buttonSizer);
  sizer->Add(buttonBox, 0, wxEXPAND);

  this->SetSizerAndFit(sizer);
}

void okTab::EmitPageChanged() {
  wxCommandEvent event(okEVT_PAGE_CHANGED, GetId());
  event.SetEventObject(this);
  event.SetInt(p->Canvas->GetPageIndex());
  ProcessWindowEvent(event);
}

okTab::~okTab() {
}

std::shared_ptr<OpenKneeboard::Tab> okTab::GetTab() const {
  return p->Tab;
}

wxImage okTab::GetImage() {
  return p->Tab->RenderPage(p->Canvas->GetPageIndex());
}
