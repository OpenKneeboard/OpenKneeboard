#include "okGameInstanceSettings.h"

#include <wx/gbsizer.h>

using namespace OpenKneeboard;

okGameInstanceSettings::okGameInstanceSettings(
  wxWindow* parent,
  const GameInstance& game)
  : wxPanel(parent, wxID_ANY), mGame(game) {
  auto grid = new wxGridBagSizer(5, 5);

  auto bold = GetFont().MakeBold();

  {
    auto label = new wxStaticText(this, wxID_ANY, _("Name"));
    label->SetFont(bold);
    grid->Add(label, wxGBPosition(0, 0));
    grid->Add(new wxStaticText(this, wxID_ANY, game.Name), wxGBPosition(0, 1));
  }

  {
    auto label = new wxStaticText(this, wxID_ANY, _("Path"));
    label->SetFont(bold);
    grid->Add(label, wxGBPosition(1, 0));
    grid->Add(
      new wxStaticText(this, wxID_ANY, game.Path.string()), wxGBPosition(1, 1));
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

GameInstance okGameInstanceSettings::GetGameInstance() const {
  return mGame;
}
