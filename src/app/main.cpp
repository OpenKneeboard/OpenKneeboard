#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/frame.h>
#pragma warning(push)
// strcopy etc may be unsafe
#pragma warning(disable : 4996)
#include <wx/notebook.h>
#pragma warning(pop)

#include "OpenKneeboard/FolderTab.h"
#include "OpenKneeboard/GameEvent.h"
#include "OpenKneeboard/Games/DCSWorld.h"
#include "OpenKneeboard/SHM.h"
#include "OpenKneeboard/dprint.h"
#include "okGameEventMailslotThread.h"
#include "okTab.h"

using namespace OpenKneeboard;

class MainWindow final : public wxFrame {
 private:
  std::vector<okTab*> mTabs;
  OpenKneeboard::SHM::Writer mSHM;

 public:
  MainWindow()
    : wxFrame(
      nullptr,
      wxID_ANY,
      "OpenKneeboard",
      wxDefaultPosition,
      wxDefaultSize,
      wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER) {
    auto menuBar = new wxMenuBar();
    {
      auto fileMenu = new wxMenu();
      menuBar->Append(fileMenu, _("&File"));

      fileMenu->Append(wxID_EXIT, _T("E&xit"));
      Bind(wxEVT_MENU, &MainWindow::OnExit, this, wxID_EXIT);
    }
    SetMenuBar(menuBar);

    auto sizer = new wxBoxSizer(wxVERTICAL);

    auto notebook = new wxNotebook(this, wxID_ANY);
    sizer->Add(notebook);

    auto tab = new okTab(
      notebook,
      std::make_shared<OpenKneeboard::FolderTab>(
        "Local",
        L"C:\\Program Files\\Eagle Dynamics\\DCS World "
        L"OpenBeta\\Mods\\terrains\\Caucasus\\Kneeboard"));
    Bind(okEVT_PAGE_CHANGED, [=](auto) { this->UpdateSHM(); });
    mTabs = {tab};
    notebook->AddPage(tab, tab->GetTab()->GetTitle());

    this->SetSizerAndFit(sizer);

    UpdateSHM();

    auto listener = new okGameEventMailslotThread(this);
    listener->Run();
    Bind(okEVT_GAME_EVENT, &MainWindow::OnGameEvent, this);
  }

  void OnGameEvent(wxThreadEvent& ev) {
    const auto payload = ev.GetPayload<GameEvent>();
    dprintf("GameEvent: '{}' = '{}'", payload.Name, payload.Value);
    // TODO
  }

  void UpdateSHM() {
    if (!mSHM) {
      return;
    }

    auto image = mTabs[0]->GetImage();
    if (!image.IsOk()) {
      return;
    }

    auto ratio = float(image.GetHeight()) / image.GetWidth();
    OpenKneeboard::SHM::Header header {
      .Flags = OpenKneeboard::Flags::DISCARD_DEPTH_INFORMATION,
      .y = 0.5f,
      .z = -0.25f,
      .rx = float(M_PI / 2),
      .VirtualWidth = 0.5f / ratio,
      .VirtualHeight = 0.5f,
      .ImageWidth = static_cast<uint16_t>(image.GetWidth()),
      .ImageHeight = static_cast<uint16_t>(image.GetHeight()),
    };

    using Pixel = OpenKneeboard::SHM::Pixel;

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
};

class OpenKneeboardApp final : public wxApp {
 public:
  virtual bool OnInit() override {
    using DCS = OpenKneeboard::Games::DCSWorld;
    dprintf(
      "DCS paths:\n  Stable: {}\n  Open Beta: {}",
      DCS::GetStablePath(),
      DCS::GetOpenBetaPath());
    wxInitAllImageHandlers();
    MainWindow* window = new MainWindow();
    window->Show();
    return true;
  }
};

wxIMPLEMENT_APP(OpenKneeboardApp);
