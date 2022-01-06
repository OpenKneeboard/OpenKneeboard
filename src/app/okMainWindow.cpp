#include "okMainWindow.h"

#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/rawbmp.h>
#include <wx/wupdlock.h>

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
#include "okOpenVR.h"

using namespace OpenKneeboard;

class okMainWindow::Impl {
 public:
  std::vector<okConfigurableComponent*> Configurables;
  std::vector<okTab*> TabUIs;
  std::unique_ptr<okOpenVR> OpenVR;
  OpenKneeboard::SHM::Writer SHM;
  wxNotebook* Notebook = nullptr;
  okTabsList* TabsList = nullptr;
  int CurrentTab = -1;
  Settings Settings = ::Settings::Load();
};

static wxImage CreateErrorImage(const wxString& text) {
  wxBitmap bm(768, 1024);
  wxMemoryDC dc(bm);

  dc.SetBrush(*wxWHITE_BRUSH);
  dc.Clear();
  RenderError(bm.GetSize(), dc, text);

  return bm.ConvertToImage();
}

okMainWindow::okMainWindow()
  : wxFrame(
    nullptr,
    wxID_ANY,
    "OpenKneeboard",
    wxDefaultPosition,
    wxDefaultSize,
    wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER),
    p(std::make_unique<Impl>()) {
  this->Bind(okEVT_GAME_EVENT, &okMainWindow::OnGameEvent, this);
  auto menuBar = new wxMenuBar();
  {
    auto fileMenu = new wxMenu();
    menuBar->Append(fileMenu, _("&File"));

    fileMenu->Append(wxID_EXIT, _("E&xit"));
    Bind(wxEVT_MENU, &okMainWindow::OnExit, this, wxID_EXIT);
  }
  {
    auto editMenu = new wxMenu();
    menuBar->Append(editMenu, _("&Edit"));

    auto settingsId = wxNewId();
    editMenu->Append(settingsId, _("&Settings..."));
    Bind(wxEVT_MENU, &okMainWindow::OnShowSettings, this, settingsId);
  }
  SetMenuBar(menuBar);

  auto sizer = new wxBoxSizer(wxVERTICAL);

  p->Notebook = new wxNotebook(this, wxID_ANY);
  p->Notebook->Bind(
    wxEVT_BOOKCTRL_PAGE_CHANGED, &okMainWindow::OnTabChanged, this);
  sizer->Add(p->Notebook);

  auto listener = new okGameEventMailslotThread(this);
  listener->Run();

  {
    auto tabs = new okTabsList(p->Settings.Tabs);
    p->TabsList = tabs;
    p->Configurables.push_back(tabs);
    UpdateTabs();
    tabs->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->p->Settings.Tabs = tabs->GetSettings();
      p->Settings.Save();
      this->UpdateTabs();
    });
  }

  this->SetSizerAndFit(sizer);

  UpdateSHM();

  {
    auto gl = new okGamesList(p->Settings.Games);
    p->Configurables.push_back(gl);

    gl->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->p->Settings.Games = gl->GetSettings();
      p->Settings.Save();
    });
  }

  {
    auto dipc = new okDirectInputController(p->Settings.DirectInput);
    p->Configurables.push_back(dipc);

    dipc->Bind(okEVT_PREVIOUS_TAB, &okMainWindow::OnPreviousTab, this);
    dipc->Bind(okEVT_NEXT_TAB, &okMainWindow::OnNextTab, this);
    dipc->Bind(okEVT_PREVIOUS_PAGE, &okMainWindow::OnPreviousPage, this);
    dipc->Bind(okEVT_NEXT_PAGE, &okMainWindow::OnNextPage, this);

    dipc->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->p->Settings.DirectInput = dipc->GetSettings();
      p->Settings.Save();
    });
  }
}

okMainWindow::~okMainWindow() {
}

void okMainWindow::OnTabChanged(wxBookCtrlEvent& ev) {
  const auto tab = ev.GetSelection();
  if (tab == wxNOT_FOUND) {
    return;
  }
  p->CurrentTab = tab;
  UpdateSHM();
}

void okMainWindow::OnGameEvent(wxThreadEvent& ev) {
  const auto ge = ev.GetPayload<GameEvent>();
  dprintf("GameEvent: '{}' = '{}'", ge.Name, ge.Value);
  for (auto tab: p->TabUIs) {
    tab->GetTab()->OnGameEvent(ge);
  }
}

void okMainWindow::UpdateSHM() {
  if (!p->SHM) {
    return;
  }

  if (p->CurrentTab < 0) {
    return;
  }

  auto tab = p->TabUIs.at(p->CurrentTab);
  auto content = tab->GetImage();
  if (!content.IsOk()) {
    if (tab->GetTab()->GetPageCount() == 0) {
      content = CreateErrorImage(_("No Pages"));
    } else {
      content = CreateErrorImage(_("Invalid Page Image"));
    }
  }

  wxSize headerSize(content.GetWidth(), int(content.GetHeight() * 0.05));
  const auto width = content.GetWidth();
  const auto height = content.GetHeight() + headerSize.GetHeight();
  wxBitmap withUI(width, height, 32);
  {
    wxMemoryDC dc(withUI);
    dc.SetBrush(*wxLIGHT_GREY_BRUSH);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(wxPoint(0, 0), headerSize);
    dc.DrawBitmap(content, wxPoint(0, headerSize.GetY()));

    wxFont font(
      wxSize(0, headerSize.GetHeight() * 0.5),
      wxFONTFAMILY_TELETYPE,
      wxFONTSTYLE_NORMAL,
      wxFONTWEIGHT_BOLD);
    dc.SetFont(font);

    auto title = tab->GetTab()->GetTitle();
    auto titleSize = dc.GetTextExtent(title);
    dc.DrawText(
      title,
      (headerSize.GetWidth() - titleSize.GetWidth()) / 2,
      (headerSize.GetHeight() - titleSize.GetHeight()) / 2);
  }

  wxAlphaPixelData pixelData(withUI);
  wxAlphaPixelData::Iterator pixelIt(pixelData);

  auto ratio = float(height) / width;
  SHM::Header header {
    .Flags = SHM::Flags::DISCARD_DEPTH_INFORMATION,
    .x = 0.15f,
    .floorY = 0.6f,
    .eyeY = -0.7f,
    .z = -0.4f,
    .rx = -float(2 * M_PI / 5),
    .rz = -float(M_PI / 32),
    .VirtualWidth = 0.25f / ratio,
    .VirtualHeight = 0.25f,
    .ZoomScale = 2.0f,
    .ImageWidth = static_cast<uint16_t>(width),
    .ImageHeight = static_cast<uint16_t>(height),
  };

  auto image = withUI.ConvertToImage();

  using Pixel = SHM::Pixel;
  std::vector<Pixel> pixels(width * height);
  auto pixelOut = &pixels[0];
  for (int y = 0; y < height; ++y) {
    pixelIt.MoveTo(pixelData, 0, y);
    for (int x = 0; x < width; ++x) {
      *pixelOut = {
        pixelIt.Red(),
        pixelIt.Green(),
        pixelIt.Blue(),
        // If we create a bitmap with an alpha channel, wxMemoryDC discards it,
        // so the alpha is always 0. For now, make it opaque instead, perhaps
        // add an option in the future.
        0xff,
      };
      pixelIt++;
      pixelOut++;
    }
  }

  p->SHM.Update(header, pixels);

  if (!p->OpenVR) {
    p->OpenVR = std::make_unique<okOpenVR>();
  }
  p->OpenVR->Update(header, pixels.data());
}

void okMainWindow::OnExit(wxCommandEvent& ev) {
  Close(true);
}

void okMainWindow::OnShowSettings(wxCommandEvent& ev) {
  auto w = new wxFrame(this, wxID_ANY, _("Settings"));
  auto s = new wxBoxSizer(wxVERTICAL);

  auto nb = new wxNotebook(w, wxID_ANY);
  s->Add(nb, 1, wxEXPAND);

  for (auto& component: p->Configurables) {
    auto p = new wxPanel(nb, wxID_ANY);
    auto ui = component->GetSettingsUI(p);
    if (!ui) {
      continue;
    }

    auto ps = new wxBoxSizer(wxVERTICAL);
    ps->Add(ui, 1, wxEXPAND, 5);
    p->SetSizerAndFit(ps);

    nb->AddPage(p, ui->GetLabel());
  }

  w->SetSizerAndFit(s);
  w->Show(true);
}

void okMainWindow::OnPreviousTab(wxCommandEvent& ev) {
  p->Notebook->AdvanceSelection(false);
}

void okMainWindow::OnNextTab(wxCommandEvent& ev) {
  p->Notebook->AdvanceSelection(true);
}

void okMainWindow::OnPreviousPage(wxCommandEvent& ev) {
  p->TabUIs[p->CurrentTab]->PreviousPage();
}

void okMainWindow::OnNextPage(wxCommandEvent& ev) {
  p->TabUIs[p->CurrentTab]->NextPage();
}

void okMainWindow::UpdateTabs() {
  auto tabs = p->TabsList->GetTabs();
  wxWindowUpdateLocker noUpdates(p->Notebook);

  auto selected
    = p->CurrentTab >= 0 ? p->TabUIs[p->CurrentTab]->GetTab() : nullptr;
  p->CurrentTab = tabs.empty() ? -1 : 0;
  p->TabUIs.clear();
  p->Notebook->DeleteAllPages();

  for (auto tab: tabs) {
    if (selected == tab) {
      p->CurrentTab = p->Notebook->GetPageCount();
    }

    auto ui = new okTab(p->Notebook, tab);
    p->TabUIs.push_back(ui);

    p->Notebook->AddPage(ui, tab->GetTitle(), selected == tab);
    ui->Bind(okEVT_TAB_PIXELS_CHANGED, [this](auto) { this->UpdateSHM(); });
  }

  UpdateSHM();
}
