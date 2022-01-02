#include "okGamesList.h"

#include <ShlObj.h>
#include <shellapi.h>
#include <wx/filedlg.h>
#include <wx/gbsizer.h>
#include <wx/listbook.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/statline.h>

#include "GenericGame.h"
#include "GetIconFromExecutable.h"
#include "OpenKneeboard/Games/DCSWorld.h"
#include "OpenKneeboard/dprint.h"
#include "okEvents.h"
#include "okSelectExecutableDialog.h"

using namespace OpenKneeboard;

okGamesList::okGamesList(const nlohmann::json& config) {
  mGames
    = {std::make_shared<Games::DCSWorld>(), std::make_shared<GenericGame>()};

  if (config.is_null()) {
    LoadDefaultSettings();
    return;
  }
  LoadSettings(config);
}

void okGamesList::LoadDefaultSettings() {
  for (const auto& game: mGames) {
    for (const auto& path: game->GetInstalledPaths()) {
      mInstances.push_back({
        .Name = game->GetUserFriendlyName(path).ToStdString(),
        .Path = path,
        .Game = game,
      });
    }
  }
}

okGamesList::~okGamesList() {
}

namespace {
class okGameInstanceSettings : public wxPanel {
 private:
  GameInstance mGame;

 public:
  okGameInstanceSettings(wxWindow* parent, const GameInstance& game)
    : wxPanel(parent, wxID_ANY), mGame(game) {
    auto grid = new wxGridBagSizer(5, 5);

    auto bold = GetFont().MakeBold();

    {
      auto label = new wxStaticText(this, wxID_ANY, _("Name"));
      label->SetFont(bold);
      grid->Add(label, wxGBPosition(0, 0));
      grid->Add(
        new wxStaticText(this, wxID_ANY, game.Name), wxGBPosition(0, 1));
    }

    {
      auto label = new wxStaticText(this, wxID_ANY, _("Path"));
      label->SetFont(bold);
      grid->Add(label, wxGBPosition(1, 0));
      grid->Add(
        new wxStaticText(this, wxID_ANY, game.Path.string()),
        wxGBPosition(1, 1));
    }

    {
      auto label = new wxStaticText(this, wxID_ANY, _("Type"));
      label->SetFont(bold);
      grid->Add(label, wxGBPosition(2, 0));
      grid->Add(
        new wxStaticText(this, wxID_ANY, game.Game->GetNameForConfigFile()),
        wxGBPosition(2, 1));
    }

    grid->AddGrowableCol(1);

    auto s = new wxBoxSizer(wxVERTICAL);
    s->Add(grid);
    s->AddStretchSpacer();
    this->SetSizerAndFit(s);
  }

  GameInstance GetGameInstance() const {
    return mGame;
  }
};

}// namespace

class okGamesList::SettingsUI final : public wxPanel {
 private:
  wxListbook* mList = nullptr;
  okGamesList* mGamesList = nullptr;

  void OnPathSelect(wxCommandEvent& ev) {
    auto path = std::filesystem::path(ev.GetString().ToStdWstring());
    int imageIndex = -1;
    auto ico = GetIconFromExecutable(path);
    auto imageList = mList->GetImageList();
    if (ico.IsOk()) {
      imageIndex = imageList->Add(ico);
    }
    for (const auto& game: mGamesList->mGames) {
      if (!game->MatchesPath(path)) {
        continue;
      }
      GameInstance instance {
        .Name = game->GetUserFriendlyName(path).ToStdString(),
        .Path = path,
        .Game = game};
      mGamesList->mInstances.push_back(instance);
      mList->AddPage(
        new okGameInstanceSettings(mList, instance),
        instance.Name,
        true,
        imageIndex);
    }
    auto dialog = dynamic_cast<wxDialog*>(ev.GetEventObject());
    if (!dialog) {
      dprintf("No dialog in {}", __FUNCTION__);
      return;
    }
    wxQueueEvent(this, new wxCommandEvent(okEVT_SETTINGS_CHANGED));
    dialog->Close();
  }

  void OnAddGameButton(wxCommandEvent& ev) {
    okSelectExecutableDialog dialog(nullptr, wxID_ANY, _("Add Game"));
    dialog.Bind(okEVT_PATH_SELECTED, &SettingsUI::OnPathSelect, this);
    dialog.ShowModal();
  }

  void OnRemoveGameButton(wxCommandEvent& ev) {
    auto page = mList->GetCurrentPage();
    if (!page) {
      return;
    }
    auto gameSettings = dynamic_cast<okGameInstanceSettings*>(page);
    if (!gameSettings) {
      dprint("Game list page is not an okGameInstanceSettings");
      return;
    }
    auto game = gameSettings->GetGameInstance();

    wxMessageDialog dialog(
      nullptr,
      fmt::format(
        _("Are you sure you want to remove '{}'?").ToStdString(), game.Name),
      _("Remove game?"),
      wxYES_NO | wxNO_DEFAULT);
    auto result = dialog.ShowModal();
    if (result != wxID_YES) {
      return;
    }

    mList->DeletePage(mList->GetSelection());

    auto& instances = mGamesList->mInstances;
    for (auto it = instances.begin(); it != instances.end(); ++it) {
      if (it->Path != game.Path) {
        continue;
      }
      instances.erase(it);
      break;
    }
    wxQueueEvent(this, new wxCommandEvent(okEVT_SETTINGS_CHANGED));
  }

 public:
  SettingsUI(wxWindow* parent, okGamesList* gamesList)
    : wxPanel(parent, wxID_ANY), mGamesList(gamesList) {
    this->SetLabel(_("Games"));
    mList = new wxListbook(this, wxID_ANY);
    mList->SetWindowStyleFlag(wxLB_LEFT);
    auto imageList = new wxImageList(32, 32);
    mList->AssignImageList(imageList);

    for (const auto& game: mGamesList->mInstances) {
      int imageIndex = -1;
      auto ico = GetIconFromExecutable(game.Path);
      if (ico.IsOk()) {
        imageIndex = imageList->Add(ico);
      }
      mList->AddPage(
        new okGameInstanceSettings(mList, game), game.Name, false, imageIndex);
    }

    auto add = new wxButton(this, wxID_ANY, _("&Add"));
    add->Bind(wxEVT_BUTTON, &SettingsUI::OnAddGameButton, this);
    auto remove = new wxButton(this, wxID_ANY, _("&Remove"));
    remove->Bind(wxEVT_BUTTON, &SettingsUI::OnRemoveGameButton, this);

    auto s = new wxBoxSizer(wxHORIZONTAL);
    s->Add(mList, 1, wxEXPAND | wxFIXED_MINSIZE, 5);
    s->Add(
      new wxStaticLine(
        this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL),
      0,
      wxEXPAND,
      5);

    auto buttons = new wxBoxSizer(wxVERTICAL);
    buttons->Add(add, 0, 0, 5);
    buttons->Add(remove, 0, 0, 5);
    buttons->AddStretchSpacer();
    s->Add(buttons, 0, wxEXPAND, 5);

    this->SetSizerAndFit(s);
  }
};

wxWindow* okGamesList::GetSettingsUI(wxWindow* parent) {
  auto ret = new SettingsUI(parent, this);
  ret->Bind(
    okEVT_SETTINGS_CHANGED, [=](auto& ev) { wxQueueEvent(this, ev.Clone()); });
  return ret;
}

nlohmann::json okGamesList::GetSettings() const {
  nlohmann::json games {};
  for (const auto& game: mInstances) {
    games.push_back(game.ToJson());
  }
  return {{"Configured", games}};
}

void okGamesList::LoadSettings(const nlohmann::json& config) {
  auto list = config.at("Configured");

  for (const auto& game: list) {
    mInstances.push_back(GameInstance::FromJson(game, mGames));
  }
}
