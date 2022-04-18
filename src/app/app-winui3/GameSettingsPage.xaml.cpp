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

#include "GameInstanceUIData.g.cpp"
#include "GameSettingsPage.g.cpp"
#include "DCSWorldInstanceUIData.g.cpp"
#include "GameInstanceUIDataTemplateSelector.g.cpp"
// clang-format on

#include <OpenKneeboard/DCSWorldInstance.h>
#include <OpenKneeboard/GamesList.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/utf8.h>
#include <fmt/format.h>
#include <microsoft.ui.xaml.window.h>
#include <shobjidl.h>
#include <winrt/windows.storage.pickers.h>

#include <algorithm>

#include "CheckDCSHooks.h"
#include "ExecutableIconFactory.h"
#include "Globals.h"

using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;

namespace winrt::OpenKneeboardApp::implementation {

GameSettingsPage::GameSettingsPage() {
  InitializeComponent();
  mIconFactory = std::make_unique<ExecutableIconFactory>();
  UpdateGames();
}

void GameSettingsPage::UpdateGames() {
  if (!mPropertyChangedEvent) {
    return;
  }
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"Games"));
}

IVector<IInspectable> GameSettingsPage::Games() {
  auto games = gKneeboard->GetGamesList()->GetGameInstances();
  std::ranges::sort(
    games, [](auto& a, auto& b) { return a->mName < b->mName; });
  IVector<IInspectable> winrtGames {
    winrt::single_threaded_vector<IInspectable>()};

  for (const auto& game: games) {
    OpenKneeboardApp::GameInstanceUIData winrtGame {nullptr};

    auto dcs = std::dynamic_pointer_cast<DCSWorldInstance>(game);
    if (dcs) {
      auto winrtDCS = winrt::make<DCSWorldInstanceUIData>();
      winrtDCS.SavedGamesPath(to_hstring(dcs->mSavedGamesPath.string()));
      winrtGame = winrtDCS;
    } else {
      winrtGame = winrt::make<GameInstanceUIData>();
    }
    winrtGame.InstanceID(game->mInstanceID);
    winrtGame.Icon(mIconFactory->CreateXamlBitmapSource(game->mPath));
    winrtGame.Name(to_hstring(game->mName));
    winrtGame.Path(game->mPath.wstring());
    winrtGame.Type(to_hstring(game->mGame->GetNameForConfigFile()));
    winrtGame.OverlayAPI(static_cast<uint8_t>(game->mOverlayAPI));

    winrtGames.Append(winrtGame);
  }

  return winrtGames;
}

GameSettingsPage::~GameSettingsPage() {
}

winrt::fire_and_forget GameSettingsPage::AddRunningProcess(
  const IInspectable&,
  const RoutedEventArgs&) {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());

  ::winrt::OpenKneeboardApp::ProcessListPage processList;
  dialog.Title(winrt::box_value(to_hstring(_("Select a Game"))));
  dialog.Content(processList);

  dialog.IsPrimaryButtonEnabled(false);
  dialog.PrimaryButtonText(to_hstring(_("Add Game")));
  dialog.CloseButtonText(to_hstring(_("Cancel")));
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

winrt::fire_and_forget GameSettingsPage::AddExe(
  const IInspectable& sender,
  const RoutedEventArgs&) {
  auto picker = Windows::Storage::Pickers::FileOpenPicker();
  picker.as<IInitializeWithWindow>()->Initialize(gMainWindow);
  picker.SettingsIdentifier(L"openkneeboard/addGameExe");
  picker.SuggestedStartLocation(
    Windows::Storage::Pickers::PickerLocationId::ComputerFolder);
  picker.FileTypeFilter().Append(L".exe");
  auto file = co_await picker.PickSingleFileAsync();
  if (!file) {
    return;
  }
  auto path = file.Path();
  if (path.empty()) {
    return;
  }

  AddPath(std::wstring_view {path});
}

static std::shared_ptr<GameInstance> GetGameInstanceFromSender(
  const IInspectable& sender) {
  const auto instanceID
    = unbox_value<uint64_t>(sender.as<FrameworkElement>().Tag());

  auto gamesList = gKneeboard->GetGamesList();
  auto instances = gamesList->GetGameInstances();
  auto it = std::find_if(instances.begin(), instances.end(), [&](auto& x) {
    return x->mInstanceID == instanceID;
  });
  if (it == instances.end()) {
    OPENKNEEBOARD_BREAK;
    return {};
  }
  return *it;
}

void GameSettingsPage::OnOverlayAPIChanged(
  const IInspectable& sender,
  const SelectionChangedEventArgs&) {
  const auto instance = GetGameInstanceFromSender(sender);

  if (!instance) {
    return;
  }

  const auto newAPI
    = static_cast<OverlayAPI>(sender.as<ComboBox>().SelectedIndex());
  if (instance->mOverlayAPI == newAPI) {
    return;
  }

  instance->mOverlayAPI = newAPI;
  gKneeboard->SaveSettings();
}

winrt::fire_and_forget GameSettingsPage::RemoveGame(
  const IInspectable& sender,
  const RoutedEventArgs&) {
  auto instance = GetGameInstanceFromSender(sender);
  if (!instance) {
    co_return;
  }

  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(
    to_hstring(fmt::format(fmt::runtime(_("Remove {}?")), instance->mName))));
  dialog.Content(box_value(to_hstring(fmt::format(
    fmt::runtime(_("Do you want OpenKneeboard to stop integrating with {}?")),
    instance->mName))));

  dialog.PrimaryButtonText(to_hstring(_("Yes")));
  dialog.CloseButtonText(to_hstring(_("No")));
  dialog.DefaultButton(ContentDialogButton::Primary);

  auto result = co_await dialog.ShowAsync();
  if (result != ContentDialogResult::Primary) {
    return;
  }

  auto gamesList = gKneeboard->GetGamesList();
  auto instances = gamesList->GetGameInstances();
  instances.erase(std::ranges::find(instances, instance));
  gamesList->SetGameInstances(instances);
  UpdateGames();
}

winrt::fire_and_forget GameSettingsPage::ChangeDCSSavedGamesPath(
  const IInspectable& sender,
  const RoutedEventArgs&) {
  auto instance = std::dynamic_pointer_cast<DCSWorldInstance>(
    GetGameInstanceFromSender(sender));
  if (!instance) {
    co_return;
  }

  hstring stringPath = co_await ChooseDCSSavedGamesFolder(
    this->XamlRoot(), DCSSavedGamesSelectionTrigger::EXPLICIT);

  if (stringPath.empty()) {
    co_return;
  }

  instance->mSavedGamesPath
    = std::filesystem::canonical(std::wstring_view {stringPath});

  CheckDCSHooks(this->XamlRoot(), instance->mSavedGamesPath);

  gKneeboard->SaveSettings();
  UpdateGames();
}

winrt::fire_and_forget GameSettingsPage::AddPath(
  const std::filesystem::path& rawPath) {
  if (rawPath.empty()) {
    co_return;
  }
  if (!std::filesystem::is_regular_file(rawPath)) {
    co_return;
  }

  std::filesystem::path path = std::filesystem::canonical(rawPath);

  auto gamesList = gKneeboard->GetGamesList();
  for (auto game: gamesList->GetGames()) {
    if (!game->MatchesPath(path)) {
      continue;
    }
    auto instance = game->CreateGameInstance(path);

    auto dcs = std::dynamic_pointer_cast<DCSWorldInstance>(instance);
    if (dcs) {
      if (dcs->mSavedGamesPath.empty()) {
        winrt::hstring stringPath = co_await ChooseDCSSavedGamesFolder(
          this->XamlRoot(), DCSSavedGamesSelectionTrigger::IMPLICIT);
        if (!stringPath.empty()) {
          dcs->mSavedGamesPath
            = std::filesystem::canonical(std::wstring_view {stringPath});
        }
      }

      if (!dcs->mSavedGamesPath.empty()) {
        co_await CheckDCSHooks(this->XamlRoot(), dcs->mSavedGamesPath);
      }
    }

    auto instances = gamesList->GetGameInstances();
    instances.push_back(instance);
    gamesList->SetGameInstances(instances);
    this->UpdateGames();
    co_return;
  }
}

GameInstanceUIData::GameInstanceUIData() {
}

BitmapSource GameInstanceUIData::Icon() {
  return mIcon;
}

void GameInstanceUIData::Icon(const BitmapSource& value) {
  mIcon = value;
}

uint64_t GameInstanceUIData::InstanceID() {
  return mInstanceID;
}

void GameInstanceUIData::InstanceID(uint64_t value) {
  mInstanceID = value;
}

hstring GameInstanceUIData::Name() {
  return mName;
}

void GameInstanceUIData::Name(const hstring& value) {
  mName = value;
}

hstring GameInstanceUIData::Path() {
  return mPath;
}

void GameInstanceUIData::Path(const hstring& value) {
  mPath = value;
}

hstring GameInstanceUIData::Type() {
  return mType;
}

void GameInstanceUIData::Type(const hstring& value) {
  mType = value;
}

uint8_t GameInstanceUIData::OverlayAPI() {
  return mOverlayAPI;
}

void GameInstanceUIData::OverlayAPI(uint8_t value) {
  mOverlayAPI = value;
}

hstring DCSWorldInstanceUIData::SavedGamesPath() {
  return mSavedGamesPath;
}

void DCSWorldInstanceUIData::SavedGamesPath(const hstring& value) {
  mSavedGamesPath = value;
}

DataTemplate GameInstanceUIDataTemplateSelector::GenericGame() {
  return mGenericGame;
}

void GameInstanceUIDataTemplateSelector::GenericGame(
  const DataTemplate& value) {
  mGenericGame = value;
}

DataTemplate GameInstanceUIDataTemplateSelector::DCSWorld() {
  return mDCSWorld;
}

void GameInstanceUIDataTemplateSelector::DCSWorld(const DataTemplate& value) {
  mDCSWorld = value;
}

DataTemplate GameInstanceUIDataTemplateSelector::SelectTemplateCore(
  const IInspectable& item) {
  if (item.try_as<winrt::OpenKneeboardApp::DCSWorldInstanceUIData>()) {
    return mDCSWorld;
  }

  return mGenericGame;
}

DataTemplate GameInstanceUIDataTemplateSelector::SelectTemplateCore(
  const IInspectable& item,
  const DependencyObject&) {
  return this->SelectTemplateCore(item);
}

winrt::event_token GameSettingsPage::PropertyChanged(
  winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const&
    handler) {
  return mPropertyChangedEvent.add(handler);
}

void GameSettingsPage::PropertyChanged(
  winrt::event_token const& token) noexcept {
  mPropertyChangedEvent.remove(token);
}

}// namespace winrt::OpenKneeboardApp::implementation
