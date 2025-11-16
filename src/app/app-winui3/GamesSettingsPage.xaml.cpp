// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
// clang-format off
#include "pch.h"

#include "GamesSettingsPage.xaml.h"

#include "GameInstanceUIData.g.cpp"
#include "GamesSettingsPage.g.cpp"
#include "DCSWorldInstanceUIData.g.cpp"
#include "GameInstanceUIDataTemplateSelector.g.cpp"
// clang-format on

#include "CheckDCSHooks.h"
#include "ExecutableIconFactory.h"
#include "FilePicker.h"
#include "Globals.h"

#include <OpenKneeboard/DCSWorldInstance.hpp>
#include <OpenKneeboard/GamesList.hpp>
#include <OpenKneeboard/KneeboardState.hpp>

#include <OpenKneeboard/utf8.hpp>

#include <microsoft.ui.xaml.window.h>

#include <algorithm>
#include <format>

#include <shobjidl.h>

using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;

namespace winrt::OpenKneeboardApp::implementation {

GamesSettingsPage::GamesSettingsPage() {
  InitializeComponent();
  mKneeboard = gKneeboard.lock();
  mIconFactory = std::make_unique<ExecutableIconFactory>();
  UpdateGames();

  AddEventListener(
    mKneeboard->GetGamesList()->evSettingsChangedEvent,
    [this]() { this->UpdateGames(); });
}

OpenKneeboard::fire_and_forget GamesSettingsPage::RestoreDefaults(
  IInspectable,
  RoutedEventArgs) noexcept {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_("Restore defaults?"))));
  dialog.Content(box_value(to_hstring(
    _("Do you want to restore the default games list, "
      "removing your preferences?"))));
  dialog.PrimaryButtonText(to_hstring(_("Restore Defaults")));
  dialog.CloseButtonText(to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Close);

  if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  co_await mKneeboard->ResetGamesSettings();
}

void GamesSettingsPage::UpdateGames() {
  mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"Games"));
}

IVector<IInspectable> GamesSettingsPage::Games() noexcept {
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
    winrtGame.OverlayAPI(static_cast<uint8_t>(game->mOverlayAPI));

    winrtGames.Append(winrtGame);
  }

  return winrtGames;
}

GamesSettingsPage::~GamesSettingsPage() noexcept {
  this->RemoveAllEventListeners();
}

OpenKneeboard::fire_and_forget GamesSettingsPage::AddRunningProcess(
  IInspectable,
  RoutedEventArgs) noexcept {
  ::winrt::OpenKneeboardApp::ProcessPickerDialog picker;
  picker.GamesOnly(true);
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

OpenKneeboard::fire_and_forget GamesSettingsPage::AddExe(
  IInspectable sender,
  RoutedEventArgs) noexcept {
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

void GamesSettingsPage::OnOverlayAPIChanged(
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

OpenKneeboard::fire_and_forget GamesSettingsPage::RemoveGame(
  IInspectable sender,
  RoutedEventArgs) noexcept {
  auto instance = GetGameInstanceFromSender(mKneeboard.get(), sender);
  if (!instance) {
    co_return;
  }

  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(
    box_value(to_hstring(std::format(_("Remove {}?"), instance->mName))));
  dialog.Content(box_value(to_hstring(
    std::format(
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

OpenKneeboard::fire_and_forget GamesSettingsPage::ChangeDCSSavedGamesPath(
  IInspectable sender,
  RoutedEventArgs) noexcept {
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
  co_await CheckDCSHooks(this->XamlRoot(), instance->mSavedGamesPath);

  mKneeboard->SaveSettings();
  UpdateGames();
}

OpenKneeboard::fire_and_forget GamesSettingsPage::AddPath(
  std::filesystem::path rawPath) {
  if (rawPath.empty()) {
    co_return;
  }
  if (!std::filesystem::is_regular_file(rawPath)) {
    co_return;
  }

  std::filesystem::path path = std::filesystem::canonical(rawPath);

  auto gamesList = mKneeboard->GetGamesList();
  const auto games = gamesList->GetGames();
  const auto game = std::ranges::find_if(
    games, [path](const auto& it) { return it->MatchesPath(path); });
  if (game == games.end()) {
    dprint.Error("Could not find a matching game for {}", path.string());
    OPENKNEEBOARD_BREAK;
    co_return;
  }
  auto instance = (*game)->CreateGameInstance(path);
  const auto correctedPattern
    = GamesList::FixPathPattern(instance->mPathPattern);
  if (!correctedPattern) {
    std::string error;
    using enum GamesList::PathPatternError;
    switch (correctedPattern.error()) {
      case NotAGame:
        error = std::format(
          _("`{}` is not a game, so is not being added to the games list.\n\n"
            "Adding things that are not games to the games list has no "
            "benefits, and can cause severe issues, including crashes and "
            "performance problems."),
          path.filename().string());
        break;
      case Launcher:
        error = std::format(
          _("`{}` is a launcher - not a game - but OpenKneeboard could not "
            "find the game. Add the game instead of the "
            "launcher."),
          path.filename().string());
        break;
    }
    ContentDialog dialog;
    dialog.XamlRoot(this->XamlRoot());
    dialog.Title(box_value(to_hstring(path.filename().string())));
    dialog.Content(box_value(to_hstring(error)));
    dialog.PrimaryButtonText(to_hstring(_("Close")));
    co_await dialog.ShowAsync();
    co_return;
  }
  if (correctedPattern != instance->mPathPattern) {
    const auto error = std::format(
      _("Adding `{0}` instead of `{1}`, as `{1}` is a launcher, not the "
        "actual game."),
      std::filesystem::path {*correctedPattern}.filename().string(),
      path.filename().string());
    ContentDialog dialog;
    dialog.XamlRoot(this->XamlRoot());
    dialog.Title(box_value(to_hstring(path.filename().string())));
    dialog.Content(box_value(to_hstring(error)));
    dialog.PrimaryButtonText(to_hstring(_("OK")));
    co_await dialog.ShowAsync();
    instance->mPathPattern = *correctedPattern;
    instance->mLastSeenPath = *correctedPattern;
    instance->mName = std::filesystem::path {*correctedPattern}.stem().string();
  }

  if (auto dcs = std::dynamic_pointer_cast<DCSWorldInstance>(instance)) {
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
