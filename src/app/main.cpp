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
#include "OpenKneeboard/RenderError.h"
#include "OpenKneeboard/SHM.h"
#include "OpenKneeboard/dprint.h"
#include "Settings.h"
#include "okDirectInputController.h"
#include "okEvents.h"
#include "okGameEventMailslotThread.h"
#include "okGamesList.h"
#include "okTab.h"
#include "okTabsList.h"

using namespace OpenKneeboard;

class MainWindow final : public wxFrame {
 private:
  std::vector<okConfigurableComponent*> mConfigurables;
  std::vector<okTab*> mTabs;
  OpenKneeboard::SHM::Writer mSHM;
  wxNotebook* mNotebook;
  int mCurrentTab = 0;
  Settings mSettings = Settings::Load();

 public:
  MainWindow()
    : wxFrame(
      nullptr,
      wxID_ANY,
      "OpenKneeboard",
      wxDefaultPosition,
      wxDefaultSize,
      wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER) {
    this->Bind(okEVT_GAME_EVENT, &MainWindow::OnGameEvent, this);
    auto menuBar = new wxMenuBar();
    {
      auto fileMenu = new wxMenu();
      menuBar->Append(fileMenu, _("&File"));

      fileMenu->Append(wxID_EXIT, _("E&xit"));
      Bind(wxEVT_MENU, &MainWindow::OnExit, this, wxID_EXIT);
    }
    {
      auto editMenu = new wxMenu();
      menuBar->Append(editMenu, _("&Edit"));

      auto settingsId = wxNewId();
      editMenu->Append(settingsId, _("&Settings..."));
      Bind(wxEVT_MENU, &MainWindow::OnShowSettings, this, settingsId);
    }
    SetMenuBar(menuBar);

    auto sizer = new wxBoxSizer(wxVERTICAL);

    mNotebook = new wxNotebook(this, wxID_ANY);
    mNotebook->Bind(
      wxEVT_BOOKCTRL_PAGE_CHANGED, &MainWindow::OnTabChanged, this);
    sizer->Add(mNotebook);

    auto listener = new okGameEventMailslotThread(this);
    listener->Run();

    {
      // TODO: settings
      auto tabs = new okTabsList(nlohmann::json {});
      for (const auto& tab: tabs->GetTabs()) {
        auto ui = new okTab(mNotebook, tab);
        mNotebook->AddPage(ui, tab->GetTitle());
        ui->Bind(okEVT_TAB_UPDATED, [this](auto) { this->UpdateSHM(); });
        ui->Bind(okEVT_PAGE_CHANGED, [this](auto) { this->UpdateSHM(); });
        mTabs.push_back(ui);
      }
    }

    this->SetSizerAndFit(sizer);

    UpdateSHM();

    {
      // TODO: settings
      auto gl = new okGamesList(nlohmann::json {});
      mConfigurables.push_back(gl);
    }

    {
      auto dipc = new okDirectInputController(mSettings.DirectInput);
      mConfigurables.push_back(dipc);

      dipc->Bind(okEVT_PREVIOUS_TAB, &MainWindow::OnPreviousTab, this);
      dipc->Bind(okEVT_NEXT_TAB, &MainWindow::OnNextTab, this);
      dipc->Bind(okEVT_PREVIOUS_PAGE, &MainWindow::OnPreviousPage, this);
      dipc->Bind(okEVT_NEXT_PAGE, &MainWindow::OnNextPage, this);

      dipc->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
        this->mSettings.DirectInput = dipc->GetSettings();
        mSettings.Save();
      });
    }
  }

  void OnTabChanged(wxBookCtrlEvent& ev) {
    const auto tab = ev.GetSelection();
    if (tab == wxNOT_FOUND) {
      return;
    }
    mCurrentTab = tab;
    UpdateSHM();
  }

  void OnGameEvent(wxThreadEvent& ev) {
    const auto ge = ev.GetPayload<GameEvent>();
    dprintf("GameEvent: '{}' = '{}'", ge.Name, ge.Value);
    for (auto tab: mTabs) {
      tab->GetTab()->OnGameEvent(ge);
    }
  }

  wxImage CreateErrorImage(const wxString& text) {
    wxBitmap bm(768, 1024);
    wxMemoryDC dc(bm);

    dc.SetBrush(*wxWHITE_BRUSH);
    dc.Clear();
    RenderError(bm.GetSize(), dc, text);

    return bm.ConvertToImage();
  }

  void UpdateSHM() {
    if (!mSHM) {
      return;
    }

    auto tab = mTabs.at(mCurrentTab);
    auto image = tab->GetImage();
    if (!image.IsOk()) {
      if (tab->GetTab()->GetPageCount() == 0) {
        image = CreateErrorImage(_("No Pages"));
      } else {
        image = CreateErrorImage(_("Invalid Page Image"));
      }
    }

    auto ratio = float(image.GetHeight()) / image.GetWidth();
    OpenKneeboard::SHM::Header header {
      .Flags = {},// OpenKneeboard::Flags::DISCARD_DEPTH_INFORMATION,
      .floorY = 0.6f,
      .eyeY = -0.7f,
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

  void OnShowSettings(wxCommandEvent& ev) {
    auto w = new wxFrame(this, wxID_ANY, _("Settings"));
    auto s = new wxBoxSizer(wxVERTICAL);

    auto nb = new wxNotebook(w, wxID_ANY);
    s->Add(nb, 1, wxEXPAND);

    for (auto& component: mConfigurables) {
      auto ui = component->GetSettingsUI(nb);
      if (!ui) {
        continue;
      }
      nb->AddPage(ui, ui->GetLabel());
    }

    w->SetSizerAndFit(s);
    w->Show(true);
  }

  void OnPreviousTab(wxCommandEvent& ev) {
    mNotebook->AdvanceSelection(false);
  }

  void OnNextTab(wxCommandEvent& ev) {
    mNotebook->AdvanceSelection(true);
  }

  void OnPreviousPage(wxCommandEvent& ev) {
    mTabs[mCurrentTab]->PreviousPage();
  }

  void OnNextPage(wxCommandEvent& ev) {
    mTabs[mCurrentTab]->NextPage();
  }
};

class OpenKneeboardApp final : public wxApp {
 public:
  virtual bool OnInit() override {
    wxInitAllImageHandlers();
    MainWindow* window = new MainWindow();
    window->Show();
    return true;
  }
};

wxIMPLEMENT_APP(OpenKneeboardApp);
