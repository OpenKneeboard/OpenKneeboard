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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
// clang-format off
#include "pch.h"

#include "GameSettingsPage.xaml.h"

#include "GameInstanceData.g.cpp"
#include "GameSettingsPage.g.cpp"
// clang-format on

#include <OpenKneeboard/GamesList.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/utf8.h>

#include <algorithm>

#include "ExecutableIconFactory.h"
#include "Globals.h"

using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {

GameSettingsPage::GameSettingsPage() {
  InitializeComponent();
  mIconFactory = std::make_unique<ExecutableIconFactory>();
  UpdateGames();
}

void GameSettingsPage::UpdateGames() {
  auto games = gKneeboard->GetGamesList()->GetGameInstances();
  std::sort(games.begin(), games.end(), [](auto& a, auto& b) {
    return a.name < b.name;
  });
  IVector<IInspectable> winrtGames {
    winrt::single_threaded_vector<IInspectable>()};
  for (const auto& game: games) {
    auto winrtGame = winrt::make<GameInstanceData>();
    winrtGame.Icon(mIconFactory->CreateXamlBitmapSource(game.path));
    winrtGame.Name(winrt::to_hstring(game.name));
    winrtGame.Path(game.path.wstring());
    winrtGames.Append(winrtGame);
  }
  List().ItemsSource(winrtGames);
}

GameSettingsPage::~GameSettingsPage() {
}

winrt::fire_and_forget GameSettingsPage::AddRunningProcess(
  const IInspectable&,
  const RoutedEventArgs&) {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());

  ::winrt::OpenKneeboardApp::ProcessListPage processList;
  dialog.Title(winrt::box_value(winrt::to_hstring(_("Select a Game"))));
  dialog.Content(processList);

  dialog.IsPrimaryButtonEnabled(false);
  dialog.PrimaryButtonText(winrt::to_hstring(_("Add")));
  dialog.CloseButtonText(winrt::to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Primary);

  processList.SelectionChanged(
    [&](auto&, auto&) { dialog.IsPrimaryButtonEnabled(true); });

  auto result = co_await dialog.ShowAsync();
  if (result != ContentDialogResult::Primary) {
    return;
  }

  auto path = processList.SelectedPath();
  if (path.empty()) {
    return;
  }

  this->AddPath(std::wstring_view {path});
}

void GameSettingsPage::AddPath(const std::filesystem::path& rawPath) {
  std::filesystem::path path = std::filesystem::canonical(rawPath);

  auto gamesList = gKneeboard->GetGamesList();
  for (auto game: gamesList->GetGames()) {
    if (!game->MatchesPath(path)) {
      continue;
    }
    GameInstance instance {
      .name = game->GetUserFriendlyName(path),
      .path = path,
      .game = game,
    };
    auto instances = gamesList->GetGameInstances();
    instances.push_back(instance);
    gamesList->SetGameInstances(instances);
    this->UpdateGames();
    return;
  }
}

GameInstanceData::GameInstanceData() {
}

BitmapSource GameInstanceData::Icon() {
  return mIcon;
}

void GameInstanceData::Icon(const BitmapSource& value) {
  mIcon = value;
}

hstring GameInstanceData::Name() {
  return mName;
}

void GameInstanceData::Name(const hstring& value) {
  mName = value;
}

hstring GameInstanceData::Path() {
  return mPath;
}

void GameInstanceData::Path(const hstring& value) {
  mPath = value;
}

}// namespace winrt::OpenKneeboardApp::implementation
