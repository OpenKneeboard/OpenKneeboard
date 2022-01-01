#include "okGamesList.h"

#include <wx/listbook.h>

#include "OpenKneeboard/Games/DCSWorld.h"

using namespace OpenKneeboard;

okGamesList::okGamesList(const nlohmann::json& config) {
  mGames = {std::make_shared<Games::DCSWorld>()};

  if (!config.is_null()) {
    // TODO
    return;
  }

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
 public:
  okGameInstanceSettings(wxWindow* parent, const GameInstance& game)
    : wxPanel(parent, wxID_ANY) {
    auto s = new wxGridSizer(2, 5, 5);

    auto bold = GetFont().MakeBold();

    {
      auto label = new wxStaticText(this, wxID_ANY, _("Name"));
      label->SetFont(bold);
      s->Add(label);
      s->Add(new wxStaticText(this, wxID_ANY, game.Name), 1);
    }

    {
      auto label = new wxStaticText(this, wxID_ANY, _("Executable"));
      label->SetFont(bold);
      s->Add(label);
      s->Add(new wxStaticText(this, wxID_ANY, game.Path.string()), 1);
    }

    s->AddStretchSpacer();
    s->AddStretchSpacer();

    SetSizerAndFit(s);
  }
};
}// namespace

wxWindow* okGamesList::GetSettingsUI(wxWindow* parent) {
  auto list = new wxListbook(parent, wxID_ANY);
  list->SetLabel(_("Games"));
  list->SetWindowStyleFlag(wxLB_LEFT);

  for (const auto& game: mInstances) {
    list->AddPage(new okGameInstanceSettings(list, game), game.Name);
  }

  return list;
}

nlohmann::json okGamesList::GetSettings() const {
  return {};
}
