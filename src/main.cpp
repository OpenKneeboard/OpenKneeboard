#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/frame.h>
#include <wx/notebook.h>

#include "TabWidget.h"
#include "YAVRK/FolderTab.h"
#include "YAVRK/SHM.h"

class MainWindow final : public wxFrame {
 private:
  std::vector<TabWidget*> mTabs;
  YAVRK::SHM::Writer mSHM;

 public:
  MainWindow()
    : wxFrame(
      nullptr,
      wxID_ANY,
      "YAVRK",
      wxDefaultPosition,
      wxDefaultSize,
      wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER) {
    auto menuBar = new wxMenuBar();
    {
      auto fileMenu = new wxMenu();
      fileMenu->Append(wxID_EXIT, _T("E&xit"));
      menuBar->Append(fileMenu, _("&File"));
    }
    SetMenuBar(menuBar);

    auto sizer = new wxBoxSizer(wxVERTICAL);

    auto notebook = new wxNotebook(this, wxID_ANY);
    sizer->Add(notebook);

    auto tab = new TabWidget(
      notebook,
      std::make_shared<YAVRK::FolderTab>(
        "Local",
        L"C:\\Program Files\\Eagle Dynamics\\DCS World "
        L"OpenBeta\\Mods\\terrains\\Caucasus\\Kneeboard"));
    tab->Bind(YAVRK_PAGE_CHANGED, [=](auto) { this->UpdateSHM(); });
    mTabs = {tab};
    notebook->AddPage(tab, tab->GetTab()->GetTitle());

    this->SetSizerAndFit(sizer);

    UpdateSHM();
  }

  void UpdateSHM() {
    if (!mSHM) {
      return;
    }

    auto image = mTabs[0]->GetImage();
    if (!image.IsOk()) {
      OutputDebugStringA("Invalid image :'(\n");
      return;
    }

    auto ratio = float(image.GetHeight()) / image.GetWidth();
    YAVRK::SHM::Header header {
      .Flags = YAVRK::Flags::DISCARD_DEPTH_INFORMATION,
      .y = 0.5f,
      .z = -0.25f,
      .rx = float(M_PI / 2),
      .VirtualWidth = 0.5f / ratio,
      .VirtualHeight = 0.5f,
      .ImageWidth = static_cast<uint16_t>(image.GetWidth()),
      .ImageHeight = static_cast<uint16_t>(image.GetHeight()),
    };

    using Pixel = YAVRK::SHM::Pixel;

    std::vector<Pixel> pixels(image.GetWidth() * image.GetHeight());
    for (int x = 0; x < image.GetWidth(); ++x) {
      for (int y = 0; y < image.GetHeight(); ++y) {
        pixels[x + (y * image.GetWidth())] = {
          image.GetRed(x, y),
          image.GetGreen(x, y),
          image.GetBlue(x, y),
          image.HasAlpha() ? image.GetAlpha(x, y) : 255ui8,
        };
      }
    }

    mSHM.Update(header, pixels);
  }

  void OnExit(wxCommandEvent& ev) {
    Close(true);
  }

  wxDECLARE_EVENT_TABLE();
};

// clang-format off
wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
  EVT_MENU(wxID_EXIT, MainWindow::OnExit)
wxEND_EVENT_TABLE();
// clang-format off

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
