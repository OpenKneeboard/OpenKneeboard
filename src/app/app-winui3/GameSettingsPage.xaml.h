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
#pragma once
// clang-format off
#include "pch.h"
#include "GameSettingsPage.g.h"
#include "GameInstanceUIData.g.h"
#include "DCSWorldInstanceUIData.g.h"
#include "GameInstanceUIDataTemplateSelector.g.h"
// clang-format on

#include <OpenKneeboard/GameInstance.h>

#include <shims/filesystem>

namespace OpenKneeboard {
class ExecutableIconFactory;
}

using namespace OpenKneeboard;

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenKneeboardApp::implementation {
struct GameSettingsPage : GameSettingsPageT<GameSettingsPage> {
  GameSettingsPage();
  ~GameSettingsPage();

  winrt::fire_and_forget RestoreDefaults(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;

  winrt::fire_and_forget AddRunningProcess(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;
  winrt::fire_and_forget RemoveGame(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;
  winrt::fire_and_forget AddExe(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;
  winrt::fire_and_forget ChangeDCSSavedGamesPath(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;

  IVector<IInspectable> Games() noexcept;

  void OnOverlayAPIChanged(
    const IInspectable&,
    const SelectionChangedEventArgs&) noexcept;

  winrt::event_token PropertyChanged(
    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const&
      handler);
  void PropertyChanged(winrt::event_token const& token) noexcept;

 private:
  void UpdateGames();
  fire_and_forget AddPath(const std::filesystem::path&);

  std::unique_ptr<OpenKneeboard::ExecutableIconFactory> mIconFactory;
  winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler>
    mPropertyChangedEvent;
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
  hstring Type();
  void Type(const hstring&);
  uint8_t OverlayAPI();
  void OverlayAPI(uint8_t);

 private:
  BitmapSource mIcon {nullptr};
  uint64_t mInstanceID;
  hstring mName;
  hstring mPath;
  hstring mType;
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
struct GameSettingsPage
  : GameSettingsPageT<GameSettingsPage, implementation::GameSettingsPage> {};

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
