#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/frame.h>
#include <wx/notebook.h>

#include "TabWidget.h"
#include "YAVRK/FolderTab.h"

class MainWindow final : public wxFrame {
 private:
  std::vector<TabWidget*> mTabs;

 public:
  MainWindow()
    : wxFrame(
      nullptr,
      wxID_ANY,
      "YAVRK",
      wxDefaultPosition,
      wxDefaultSize,
      wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER) {
    auto sizer = new wxBoxSizer(wxVERTICAL);
    
    auto notebook = new wxNotebook(this, wxID_ANY);
    sizer->Add(notebook);

    auto tab = new TabWidget(
      this,
      std::make_shared<YAVRK::FolderTab>(
        "Local",
        L"C:\\Program Files\\Eagle Dynamics\\DCS World "
        L"OpenBeta\\Mods\\terrains\\Caucasus\\Kneeboard"));
    mTabs = {tab};
    notebook->AddPage(tab, tab->GetTab()->GetTitle());

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
