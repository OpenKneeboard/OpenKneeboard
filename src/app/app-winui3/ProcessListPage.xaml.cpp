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
#include "ProcessListPage.xaml.h"
#include "ProcessListPage.g.cpp"
// clang-format on

#include <TlHelp32.h>

#include <algorithm>
#include <filesystem>
#include <set>

#include "ExecutableIconFactory.h"

using namespace winrt::Windows::Foundation::Collections;
using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

static std::filesystem::path GetFullPathFromPID(DWORD pid) {
  winrt::handle process {
    OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid)};
  if (!process) {
    return {};
  }

  wchar_t path[MAX_PATH];
  DWORD pathLen = MAX_PATH;
  if (!QueryFullProcessImageNameW(process.get(), 0, &path[0], &pathLen)) {
    return {};
  }

  return std::wstring_view(&path[0], pathLen);
}

ProcessListPage::ProcessListPage() {
  InitializeComponent();
  ExecutableIconFactory iconFactory;

  winrt::handle snapshot {CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
  PROCESSENTRY32 process;
  process.dwSize = sizeof(process);
  Process32First(snapshot.get(), &process);
  std::set<std::filesystem::path> seen;
  std::vector<GameInstanceUIData> games;
  do {
    auto path = GetFullPathFromPID(process.th32ProcessID);
    if (path.empty()) {
      continue;
    }
    if (seen.contains(path)) {
      continue;
    }
    seen.emplace(path);
    GameInstanceUIData game;
    game.Path(path.wstring());
    game.Icon(iconFactory.CreateXamlBitmapSource(path));
    game.Name(path.stem().wstring());

    games.push_back(game);
  } while (Process32Next(snapshot.get(), &process));

  std::sort(games.begin(), games.end(), [](const auto& a, const auto& b) {
    return a.Name() < b.Name();
  });

  auto winrtGames {winrt::single_threaded_vector<IInspectable>()};
  for (const auto& game: games) {
    winrtGames.Append(game);
  }

  List().ItemsSource(winrtGames);
}

ProcessListPage::~ProcessListPage() {
}

hstring ProcessListPage::SelectedPath() {
  return mSelectedPath;
}

void ProcessListPage::OnListSelectionChanged(
  const IInspectable&,
  const SelectionChangedEventArgs& args) {
  if (args.AddedItems().Size() == 0) {
    mSelectedPath = {};
  } else {
    auto selected = args.AddedItems().GetAt(0).as<GameInstanceUIData>();
    mSelectedPath = selected.Path();
  }
  mSelectionChangedEvent(*this, mSelectedPath);
}

winrt::event_token ProcessListPage::SelectionChanged(
  winrt::Windows::Foundation::EventHandler<hstring> const& handler) {
  return mSelectionChangedEvent.add(handler);
}

void ProcessListPage::SelectionChanged(
  winrt::event_token const& token) noexcept {
  mSelectionChangedEvent.remove(token);
}
}// namespace winrt::OpenKneeboardApp::implementation
