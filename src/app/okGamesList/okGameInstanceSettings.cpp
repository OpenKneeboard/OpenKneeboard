/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
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
    grid->Add(new wxStaticText(this, wxID_ANY, game.name), wxGBPosition(0, 1));
  }

  {
    auto label = new wxStaticText(this, wxID_ANY, _("Path"));
    label->SetFont(bold);
    grid->Add(label, wxGBPosition(1, 0));
    grid->Add(
      new wxStaticText(this, wxID_ANY, game.path.wstring()), wxGBPosition(1, 1));
  }

  {
    auto label = new wxStaticText(this, wxID_ANY, _("Type"));
    label->SetFont(bold);
    grid->Add(label, wxGBPosition(2, 0));
    grid->Add(
      new wxStaticText(this, wxID_ANY, game.game->GetNameForConfigFile()),
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
