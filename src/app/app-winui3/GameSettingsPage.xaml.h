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
#include "GameInstanceData.g.h"
// clang-format on

#include <OpenKneeboard/GameInstance.h>

struct IWICImagingFactory;

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;

namespace winrt::OpenKneeboardApp::implementation {
struct GameSettingsPage : GameSettingsPageT<GameSettingsPage> {
  GameSettingsPage();
  ~GameSettingsPage();
  private:
   BitmapSource GetIconFromExecutable(const std::filesystem::path&);
   winrt::com_ptr<IWICImagingFactory> mWIC;
};

struct GameInstanceData : GameInstanceDataT<GameInstanceData> {
  GameInstanceData();

  BitmapSource Icon();
  void Icon(const BitmapSource&);
  hstring Name();
  void Name(const hstring&);
  hstring Path();
  void Path(const hstring&);

 private:
  BitmapSource mIcon { nullptr };
  hstring mName;
  hstring mPath;
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct GameSettingsPage
  : GameSettingsPageT<GameSettingsPage, implementation::GameSettingsPage> {};

struct GameInstanceData
  : GameInstanceDataT<GameInstanceData, implementation::GameInstanceData> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
