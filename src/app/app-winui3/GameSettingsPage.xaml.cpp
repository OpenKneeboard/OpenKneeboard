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
#include <wincodec.h>

#include <algorithm>

#include "Globals.h"

using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenKneeboardApp::implementation {

GameSettingsPage::GameSettingsPage() {
  InitializeComponent();
  mWIC = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory);

  auto games = gKneeboard->GetGamesList()->GetGameInstances();
  std::sort(games.begin(), games.end(), [](auto& a, auto& b) {
    return a.name < b.name;
  });
  IVector<IInspectable> winrtGames {
    winrt::single_threaded_vector<IInspectable>()};
  for (const auto& game: games) {
    auto winrtGame = winrt::make<GameInstanceData>();
    winrtGame.Icon(GetIconFromExecutable(game.path));
    winrtGame.Name(winrt::to_hstring(game.name));
    winrtGame.Path(game.path.wstring());
    winrtGames.Append(winrtGame);
  }
  List().ItemsSource(winrtGames);
}

GameSettingsPage::~GameSettingsPage() {
}

BitmapSource GameSettingsPage::GetIconFromExecutable(
  const std::filesystem::path& path) {
  auto wpath = path.wstring();
  HICON handle = ExtractIcon(GetModuleHandle(NULL), wpath.c_str(), 0);
  if (!handle) {
    handle = LoadIconW(NULL, IDI_APPLICATION);
  }
  winrt::com_ptr<IWICBitmap> wicBitmap;
  winrt::check_hresult(mWIC->CreateBitmapFromHICON(handle, wicBitmap.put()));
  UINT width, height;
  wicBitmap->GetSize(&width, &height);
  DestroyIcon(handle);

  WriteableBitmap xamlBitmap(width, height);
  wicBitmap->CopyPixels(
    nullptr,
    width * 4,
    height * width * 4,
    reinterpret_cast<BYTE*>(xamlBitmap.PixelBuffer().data()));
  return xamlBitmap;
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
