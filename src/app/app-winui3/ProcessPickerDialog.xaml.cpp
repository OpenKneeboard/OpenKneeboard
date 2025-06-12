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
#include "ProcessPickerDialog.xaml.h"
#include "ProcessPickerDialog.g.cpp"
// clang-format on

#include "ExecutableIconFactory.h"

#include <OpenKneeboard/GamesList.hpp>

#include <OpenKneeboard/utf8.hpp>

#include <TlHelp32.h>

#include <algorithm>
#include <filesystem>
#include <set>

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

ProcessPickerDialog::ProcessPickerDialog() {
  InitializeComponent();
  this->Reload();
}

bool ProcessPickerDialog::GamesOnly() const noexcept {
  return mGamesOnly;
}

void ProcessPickerDialog::GamesOnly(bool value) noexcept {
  if (mGamesOnly == value) {
    return;
  }
  mGamesOnly = value;
  this->Reload();
}

void ProcessPickerDialog::Reload() {
  ExecutableIconFactory iconFactory;

  winrt::handle snapshot {CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
  PROCESSENTRY32 process;
  process.dwSize = sizeof(process);
  Process32First(snapshot.get(), &process);
  std::set<std::filesystem::path> seen;
  std::vector<IInspectable> games;
  do {
    auto path = GetFullPathFromPID(process.th32ProcessID);
    if (path.empty()) {
      continue;
    }
    if (mGamesOnly) {
      const auto utf8 = path.string();
      const auto corrected = GamesList::FixPathPattern(utf8);
      if (!corrected) {
        continue;
      }
      if (corrected != utf8) {
        path = corrected.value();
      }
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

  std::ranges::sort(games, {}, [](const auto& it) {
    return it.as<GameInstanceUIData>().Name();
  });

  mProcesses = games;
  List().ItemsSource(single_threaded_vector(std::move(games)));
}

ProcessPickerDialog::~ProcessPickerDialog() {
}

hstring ProcessPickerDialog::SelectedPath() noexcept {
  return mSelectedPath;
}

void ProcessPickerDialog::OnListSelectionChanged(
  const IInspectable&,
  const SelectionChangedEventArgs& args) noexcept {
  if (args.AddedItems().Size() == 0) {
    mSelectedPath = {};
    this->IsPrimaryButtonEnabled(false);
  } else {
    auto selected = args.AddedItems().GetAt(0).as<GameInstanceUIData>();
    mSelectedPath = selected.Path();
    this->IsPrimaryButtonEnabled(true);
  }
}

void ProcessPickerDialog::OnAutoSuggestTextChanged(
  const AutoSuggestBox& box,
  const AutoSuggestBoxTextChangedEventArgs& args) noexcept {
  if (args.Reason() != AutoSuggestionBoxTextChangeReason::UserInput) {
    return;
  }

  const std::wstring_view queryText {box.Text()};
  if (queryText.empty()) {
    box.ItemsSource({nullptr});
    if (mFiltered) {
      List().ItemsSource(single_threaded_vector(
        std::vector<IInspectable> {mProcesses.begin(), mProcesses.end()}));
      mFiltered = false;
    }
    return;
  }

  auto matching = this->GetFilteredProcesses(queryText);

  std::ranges::sort(matching, {}, [](const auto& inspectable) {
    return fold_utf8(
      to_string(inspectable.as<OpenKneeboardApp::GameInstanceUIData>()
                  .GetStringRepresentation()));
  });

  box.ItemsSource(single_threaded_vector(std::move(matching)));
}

std::vector<IInspectable> ProcessPickerDialog::GetFilteredProcesses(
  std::wstring_view queryText) {
  if (queryText.empty()) {
    return mProcesses;
  }

  const auto folded = fold_utf8(to_utf8(queryText));
  auto words = std::string_view {folded}
    | std::views::split(std::string_view {" "})
    | std::views::transform([](auto subRange) {
                 return std::string_view {subRange.begin(), subRange.end()};
               });

  std::vector<IInspectable> ret;
  for (auto rawData: mProcesses) {
    auto process = rawData.as<OpenKneeboardApp::GameInstanceUIData>();
    // The 'name' is just the file basename
    const auto path = fold_utf8(winrt::to_string(process.Path()));

    bool matchedAllWords = true;
    for (auto word: words) {
      if (path.find(word) != path.npos) {
        continue;
      }
      matchedAllWords = false;
      break;
    }
    if (matchedAllWords) {
      ret.push_back(process);
    }
  }

  return ret;
}

void ProcessPickerDialog::OnAutoSuggestQuerySubmitted(
  const AutoSuggestBox& box,
  const AutoSuggestBoxQuerySubmittedEventArgs& args) noexcept {
  if (auto it = args.ChosenSuggestion()) {
    List().ItemsSource(single_threaded_vector<IInspectable>({it}));
    List().SelectedItem(it);
    return;
  }

  List().ItemsSource(
    single_threaded_vector(this->GetFilteredProcesses(args.QueryText())));

  mFiltered = true;
}

}// namespace winrt::OpenKneeboardApp::implementation
