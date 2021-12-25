#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/frame.h>

#include "TabCanvasWidget.h"
#include "YAVRK/FolderTab.h"

class MainWindow final : public wxFrame {
 private:
  std::vector<TabCanvasWidget*> mTabs;

 public:
  MainWindow()
    : wxFrame(
      nullptr,
      wxID_ANY,
      "YAVRK",
      wxDefaultPosition,
      wxDefaultSize,
      wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER) {
    auto tab = new TabCanvasWidget(
      this,
      std::make_shared<YAVRK::FolderTab>(
        "Local",
        L"C:\\Program Files\\Eagle Dynamics\\DCS World "
        L"OpenBeta\\Mods\\terrains\\Caucasus\\Kneeboard"));
    mTabs = {tab};

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(tab);

    auto buttonBox = new wxPanel(this);
    auto previousPage = new wxButton(buttonBox, wxID_ANY, _("&Previous Page"));
    auto nextPage = new wxButton(buttonBox, wxID_ANY, _("&Next Page"));
    previousPage->Bind(wxEVT_BUTTON, [=](auto) { tab->PreviousPage(); });
    nextPage->Bind(wxEVT_BUTTON, [=](auto) { tab->NextPage(); });

    auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(previousPage);
    buttonSizer->AddStretchSpacer();
    buttonSizer->Add(nextPage);
    buttonBox->SetSizer(buttonSizer);
    sizer->Add(buttonBox, 0, wxEXPAND);

    this->SetSizerAndFit(sizer);
  }
};

class YAVRKApp final : public wxApp {
 public:
  virtual bool OnInit() override {
    wxInitAllImageHandlers();
    MainWindow* window = new MainWindow();
    window->Show();
    return true;
  }
};

wxIMPLEMENT_APP(YAVRKApp);
