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

#include "CheckDCSHooks.h"
#include "ExecutableIconFactory.h"
#include "FilePicker.h"
#include "Globals.h"

#include <OpenKneeboard/DCSWorldInstance.h>
#include <OpenKneeboard/GamesList.h>
#include <OpenKneeboard/KneeboardState.h>

#include <OpenKneeboard/utf8.h>

#include <microsoft.ui.xaml.window.h>

#include <algorithm>
#include <format>

#include <shobjidl.h>

using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;

namespace winrt::OpenKneeboardApp::implementation {

GameSettingsPage::GameSettingsPage() {
  InitializeComponent();
  mKneeboard = gKneeboard.lock();
  mIconFactory = std::make_unique<ExecutableIconFactory>();
  UpdateGames();

  AddEventListener(
    mKneeboard->GetGamesList()->evSettingsChangedEvent,
    [this]() { this->UpdateGames(); });
}

fire_and_forget GameSettingsPage::RestoreDefaults(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_("Restore defaults?"))));
  dialog.Content(
    box_value(to_hstring(_("Do you want to restore the default games list, "
                           "removing your preferences?"))));
  dialog.PrimaryButtonText(to_hstring(_("Restore Defaults")));
  dialog.CloseButtonText(to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Close);

  if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  mKneeboard->ResetGamesSettings();
}

void GameSettingsPage::UpdateGames() {
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"Games"));
}

IVector<IInspectable> GameSettingsPage::Games() noexcept {
  auto games = mKneeboard->GetGamesList()->GetGameInstances();
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
    winrtGame.Icon(mIconFactory->CreateXamlBitmapSource(game->mLastSeenPath));
    winrtGame.Name(to_hstring(game->mName));
    winrtGame.Path(to_hstring(game->mPathPattern));
    winrtGame.Type(to_hstring(game->mGame->GetNameForConfigFile()));
    winrtGame.OverlayAPI(static_cast<uint8_t>(game->mOverlayAPI));

    winrtGames.Append(winrtGame);
  }

  return winrtGames;
}

GameSettingsPage::~GameSettingsPage() noexcept {
  this->RemoveAllEventListeners();
}

winrt::fire_and_forget GameSettingsPage::AddRunningProcess(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  ::winrt::OpenKneeboardApp::ProcessPickerDialog picker;
  picker.XamlRoot(this->XamlRoot());

  auto result = co_await picker.ShowAsync();
  if (result != ContentDialogResult::Primary) {
    co_return;
  }

  auto path = picker.SelectedPath();
  if (path.empty()) {
    co_return;
  }

  this->AddPath(std::wstring_view {path});
}

winrt::fire_and_forget GameSettingsPage::AddExe(
  const IInspectable& sender,
  const RoutedEventArgs&) noexcept {
  constexpr winrt::guid thisCall {
    0x01944f0a,
    0x58a5,
    0x42ca,
    {0xb1, 0x45, 0x6e, 0xf5, 0x07, 0x2b, 0xab, 0x34}};

  FilePicker picker(gMainWindow);
  picker.SettingsIdentifier(thisCall);
  picker.SuggestedStartLocation(FOLDERID_ProgramFiles);
  picker.AppendFileType(L"Application", {L".exe"});
  auto file = picker.PickSingleFile();
  if (!file) {
    co_return;
  }

  AddPath(*file);
}

static std::shared_ptr<GameInstance> GetGameInstanceFromSender(
  KneeboardState* kneeboard,
  const IInspectable& sender) noexcept {
  const auto instanceID
    = unbox_value<uint64_t>(sender.as<FrameworkElement>().Tag());

  auto gamesList = kneeboard->GetGamesList();
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
  const SelectionChangedEventArgs&) noexcept {
  const auto instance = GetGameInstanceFromSender(mKneeboard.get(), sender);

  if (!instance) {
    return;
  }

  const auto newAPI
    = static_cast<OverlayAPI>(sender.as<ComboBox>().SelectedIndex());
  if (instance->mOverlayAPI == newAPI) {
    return;
  }

  instance->mOverlayAPI = newAPI;
  mKneeboard->SaveSettings();
}

winrt::fire_and_forget GameSettingsPage::RemoveGame(
  const IInspectable& sender,
  const RoutedEventArgs&) noexcept {
  auto instance = GetGameInstanceFromSender(mKneeboard.get(), sender);
  if (!instance) {
    co_return;
  }

  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(
    box_value(to_hstring(std::format(_("Remove {}?"), instance->mName))));
  dialog.Content(box_value(to_hstring(std::format(
    _("Do you want OpenKneeboard to stop integrating with {}?"),
    instance->mName))));

  dialog.PrimaryButtonText(to_hstring(_("Yes")));
  dialog.CloseButtonText(to_hstring(_("No")));
  dialog.DefaultButton(ContentDialogButton::Primary);

  auto result = co_await dialog.ShowAsync();
  if (result != ContentDialogResult::Primary) {
    co_return;
  }

  auto gamesList = mKneeboard->GetGamesList();
  auto instances = gamesList->GetGameInstances();
  instances.erase(std::ranges::find(instances, instance));
  gamesList->SetGameInstances(instances);
  UpdateGames();
}

winrt::fire_and_forget GameSettingsPage::ChangeDCSSavedGamesPath(
  const IInspectable& sender,
  const RoutedEventArgs&) noexcept {
  auto instance = std::dynamic_pointer_cast<DCSWorldInstance>(
    GetGameInstanceFromSender(mKneeboard.get(), sender));
  if (!instance) {
    co_return;
  }

  const auto path = co_await ChooseDCSSavedGamesFolder(
    this->XamlRoot(), DCSSavedGamesSelectionTrigger::EXPLICIT);

  if (!path) {
    co_return;
  }

  instance->mSavedGamesPath = *path;
  CheckDCSHooks(this->XamlRoot(), instance->mSavedGamesPath);

  mKneeboard->SaveSettings();
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

  auto gamesList = mKneeboard->GetGamesList();
  for (auto game: gamesList->GetGames()) {
    if (!game->MatchesPath(path)) {
      continue;
    }
    auto instance = game->CreateGameInstance(path);

    auto dcs = std::dynamic_pointer_cast<DCSWorldInstance>(instance);
    if (dcs) {
      if (dcs->mSavedGamesPath.empty()) {
        const auto path = co_await ChooseDCSSavedGamesFolder(
          this->XamlRoot(), DCSSavedGamesSelectionTrigger::IMPLICIT);
        if (path) {
          dcs->mSavedGamesPath = *path;
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

hstring DCSWorldInstanceUIData::SavedGamesPath() noexcept {
  return mSavedGamesPath;
}

void DCSWorldInstanceUIData::SavedGamesPath(const hstring& value) noexcept {
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

}// namespace winrt::OpenKneeboardApp::implementation
