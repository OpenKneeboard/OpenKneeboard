// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once
// clang-format off
#include "pch.h"
#include "GamesSettingsPage.g.h"
#include "GameInstanceUIData.g.h"
#include "DCSWorldInstanceUIData.g.h"
#include "GameInstanceUIDataTemplateSelector.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/GameInstance.hpp>

#include <filesystem>

namespace OpenKneeboard {
class ExecutableIconFactory;
class KneeboardState;
}// namespace OpenKneeboard

using namespace OpenKneeboard;

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Data;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenKneeboardApp::implementation {
struct GamesSettingsPage
  : GamesSettingsPageT<GamesSettingsPage>,
    OpenKneeboard::WithPropertyChangedEventOnProfileChange<GamesSettingsPage> {
  GamesSettingsPage();
  ~GamesSettingsPage() noexcept;

  OpenKneeboard::fire_and_forget RestoreDefaults(
    IInspectable,
    RoutedEventArgs) noexcept;

  OpenKneeboard::fire_and_forget AddRunningProcess(
    IInspectable,
    RoutedEventArgs) noexcept;
  OpenKneeboard::fire_and_forget RemoveGame(
    IInspectable,
    RoutedEventArgs) noexcept;
  OpenKneeboard::fire_and_forget AddExe(IInspectable, RoutedEventArgs) noexcept;
  OpenKneeboard::fire_and_forget ChangeDCSSavedGamesPath(
    IInspectable,
    RoutedEventArgs) noexcept;

  IVector<IInspectable> Games() noexcept;

  void OnOverlayAPIChanged(
    const IInspectable&,
    const SelectionChangedEventArgs&) noexcept;

 private:
  void UpdateGames();
  OpenKneeboard::fire_and_forget AddPath(std::filesystem::path);

  std::unique_ptr<OpenKneeboard::ExecutableIconFactory> mIconFactory;
  std::shared_ptr<KneeboardState> mKneeboard;
};

struct GameInstanceUIData : GameInstanceUIDataT<GameInstanceUIData> {
  GameInstanceUIData();

  uint64_t InstanceID();
  void InstanceID(uint64_t);

  BitmapSource Icon();
  void Icon(const BitmapSource&);
  hstring Name();
  void Name(const hstring&);
  hstring Path();
  void Path(const hstring&);
  uint8_t OverlayAPI();
  void OverlayAPI(uint8_t);

  // ICustomPropertyProvider
  auto Type() const {
    return xaml_typename<OpenKneeboardApp::GameInstanceUIData>();
  }

  auto GetCustomProperty(const auto&) {
    return ICustomProperty {nullptr};
  }

  auto GetIndexedProperty(const auto&, const auto&) {
    return ICustomProperty {nullptr};
  }

  inline hstring GetStringRepresentation() const {
    return mName;
  }

 private:
  BitmapSource mIcon {nullptr};
  uint64_t mInstanceID;
  hstring mName;
  hstring mPath;
  uint8_t mOverlayAPI;
};

struct DCSWorldInstanceUIData
  : DCSWorldInstanceUIDataT<
      DCSWorldInstanceUIData,
      OpenKneeboardApp::implementation::GameInstanceUIData> {
  DCSWorldInstanceUIData() = default;

  hstring SavedGamesPath() noexcept;
  void SavedGamesPath(const hstring&) noexcept;

 private:
  hstring mSavedGamesPath;
};

struct GameInstanceUIDataTemplateSelector
  : GameInstanceUIDataTemplateSelectorT<GameInstanceUIDataTemplateSelector> {
  GameInstanceUIDataTemplateSelector() = default;

  DataTemplate GenericGame();
  void GenericGame(const DataTemplate&);
  DataTemplate DCSWorld();
  void DCSWorld(const DataTemplate&);

  DataTemplate SelectTemplateCore(const IInspectable&);
  DataTemplate SelectTemplateCore(const IInspectable&, const DependencyObject&);

 private:
  DataTemplate mGenericGame;
  DataTemplate mDCSWorld;
};

}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct GamesSettingsPage
  : GamesSettingsPageT<GamesSettingsPage, implementation::GamesSettingsPage> {};

struct GameInstanceUIData : GameInstanceUIDataT<
                              GameInstanceUIData,
                              implementation::GameInstanceUIData> {};

struct DCSWorldInstanceUIData : DCSWorldInstanceUIDataT<
                                  DCSWorldInstanceUIData,
                                  implementation::DCSWorldInstanceUIData> {};

struct GameInstanceUIDataTemplateSelector
  : GameInstanceUIDataTemplateSelectorT<
      GameInstanceUIDataTemplateSelector,
      implementation::GameInstanceUIDataTemplateSelector> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
