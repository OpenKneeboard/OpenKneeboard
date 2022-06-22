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
#include "TabSettingsPage.xaml.h"
#include "TabUIData.g.cpp"
#include "TabSettingsPage.g.cpp"
// clang-format on

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/TabTypes.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/dprint.h>
#include <microsoft.ui.xaml.window.h>
#include <shobjidl.h>
#include <winrt/windows.storage.pickers.h>

#include <ranges>

#include "Globals.h"

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {

TabSettingsPage::TabSettingsPage() {
  InitializeComponent();

  auto tabs = winrt::single_threaded_observable_vector<IInspectable>();
  for (const auto& tab: gKneeboard->GetTabs()) {
    auto winrtTab = winrt::make<TabUIData>();
    winrtTab.Title(to_hstring(tab->GetTitle()));
    winrtTab.InstanceID(tab->GetRuntimeID().GetTemporaryValue());
    tabs.Append(winrtTab);
  }
  List().ItemsSource(tabs);
  tabs.VectorChanged({this, &TabSettingsPage::OnTabsChanged});

  auto tabTypes = AddTabFlyout().Items();
#define IT(label, name) \
  MenuFlyoutItem name##Item; \
  name##Item.Text(to_hstring(label)); \
  name##Item.Tag(box_value<uint64_t>(TABTYPE_IDX_##name)); \
  name##Item.Click({this, &TabSettingsPage::CreateTab}); \
  tabTypes.Append(name##Item);
  OPENKNEEBOARD_TAB_TYPES
#undef IT
}

fire_and_forget TabSettingsPage::RemoveTab(
  const IInspectable& sender,
  const RoutedEventArgs&) {
  const auto tabID = Tab::RuntimeID::FromTemporaryValue(
    unbox_value<uint64_t>(sender.as<Button>().Tag()));
  const auto tabs = gKneeboard->GetTabs();
  auto it = std::ranges::find(tabs, tabID, &Tab::GetRuntimeID);
  if (it == tabs.end()) {
    co_return;
  }
  const auto& tab = *it;

  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(
    box_value(to_hstring(std::format(_("Remove {}?"), tab->GetTitle()))));
  dialog.Content(box_value(to_hstring(
    std::format(_("Do you want to remove the '{}' tab?"), tab->GetTitle()))));

  dialog.PrimaryButtonText(to_hstring(_("Yes")));
  dialog.CloseButtonText(to_hstring(_("No")));
  dialog.DefaultButton(ContentDialogButton::Primary);

  auto result = co_await dialog.ShowAsync();
  if (result != ContentDialogResult::Primary) {
    co_return;
  }

  auto idx = static_cast<uint8_t>(it - tabs.begin());
  gKneeboard->RemoveTab(idx);
  List()
    .ItemsSource()
    .as<Windows::Foundation::Collections::IVector<IInspectable>>()
    .RemoveAt(idx);
}

void TabSettingsPage::CreateTab(
  const IInspectable& sender,
  const RoutedEventArgs&) noexcept {
  auto tabType = static_cast<TabType>(
    unbox_value<uint64_t>(sender.as<MenuFlyoutItem>().Tag()));
  switch (tabType) {
    case TabType::Folder:
      CreateFolderBasedTab<FolderTab>();
      return;
    case TabType::PDF:
      CreateFileBasedTab<PDFTab>(L".pdf");
      return;
    case TabType::TextFile:
      CreateFileBasedTab<TextFileTab>(L".txt");
      return;
  }
#define IT(_, type) \
  if (tabType == TabType::type) { \
    if constexpr (std::constructible_from< \
                    type##Tab, \
                    DXResources, \
                    KneeboardState*>) { \
      AddTab(std::make_shared<type##Tab>(gDXResources, gKneeboard.get())); \
      return; \
    } else { \
      throw std::logic_error( \
        std::format("Don't know how to construct {}Tab", #type)); \
    } \
  }
  OPENKNEEBOARD_TAB_TYPES
#undef IT
  throw std::logic_error(
    std::format("Unhandled tab type: {}", static_cast<uint8_t>(tabType)));
}

template <class T>
fire_and_forget TabSettingsPage::CreateFileBasedTab(hstring fileExtension) {
  auto picker = Windows::Storage::Pickers::FileOpenPicker();
  picker.as<IInitializeWithWindow>()->Initialize(gMainWindow);
  picker.SettingsIdentifier(L"chooseFileForTab");
  picker.SuggestedStartLocation(
    Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
  picker.FileTypeFilter().Append(fileExtension);

  auto file = co_await picker.PickSingleFileAsync();
  if (!file) {
    co_return;
  }
  auto stringPath = file.Path();
  if (stringPath.empty()) {
    co_return;
  }

  const auto path = std::filesystem::canonical(std::wstring_view {stringPath});
  const auto title = path.stem();

  this->AddTab(
    std::make_shared<T>(gDXResources, gKneeboard.get(), title, path));
}

template <class T>
fire_and_forget TabSettingsPage::CreateFolderBasedTab() {
  auto picker = Windows::Storage::Pickers::FolderPicker();
  picker.as<IInitializeWithWindow>()->Initialize(gMainWindow);
  picker.SettingsIdentifier(L"chooseFileForTab");
  picker.FileTypeFilter().Append(L"*");
  picker.SuggestedStartLocation(
    Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);

  auto folder = co_await picker.PickSingleFolderAsync();
  if (!folder) {
    co_return;
  }
  auto stringPath = folder.Path();
  if (stringPath.empty()) {
    co_return;
  }

  const auto path = std::filesystem::canonical(std::wstring_view {stringPath});
  const auto title = path.stem();

  this->AddTab(
    std::make_shared<T>(gDXResources, gKneeboard.get(), title, path));
  co_return;
}

void TabSettingsPage::AddTab(const std::shared_ptr<Tab>& tab) {
  auto idx = List().SelectedIndex();
  if (idx == -1) {
    idx = 0;
  }
  gKneeboard->InsertTab(static_cast<uint8_t>(idx), tab);

  auto winrtTab = winrt::make<TabUIData>();
  winrtTab.Title(to_hstring(tab->GetTitle()));
  winrtTab.InstanceID(tab->GetRuntimeID().GetTemporaryValue());
  List()
    .ItemsSource()
    .as<Windows::Foundation::Collections::IVector<IInspectable>>()
    .InsertAt(idx, winrtTab);
}

void TabSettingsPage::OnTabsChanged(
  const IInspectable&,
  const Windows::Foundation::Collections::IVectorChangedEventArgs&) noexcept {
  // For add/remove, the kneeboard state is updated first, but for reorder,
  // the ListView is the source of truth.
  //
  // Reorders are two-step: a remove and an insert
  auto items = List()
                 .ItemsSource()
                 .as<Windows::Foundation::Collections::IVector<IInspectable>>();
  auto tabs = gKneeboard->GetTabs();
  if (items.Size() != tabs.size()) {
    // ignore the deletion ...
    return;
  }
  // ... but act on the insert :)

  std::vector<std::shared_ptr<Tab>> reorderedTabs;
  for (auto item: items) {
    auto id
      = Tab::RuntimeID::FromTemporaryValue(item.as<TabUIData>()->InstanceID());
    auto it = std::ranges::find(tabs, id, &Tab::GetRuntimeID);
    if (it == tabs.end()) {
      continue;
    }
    reorderedTabs.push_back(*it);
  }
  gKneeboard->SetTabs(reorderedTabs);
}

hstring TabUIData::Title() {
  return mTitle;
}

void TabUIData::Title(hstring value) {
  mTitle = value;
}

uint64_t TabUIData::InstanceID() {
  return mInstanceID;
}

void TabUIData::InstanceID(uint64_t value) {
  mInstanceID = value;
}

}// namespace winrt::OpenKneeboardApp::implementation
