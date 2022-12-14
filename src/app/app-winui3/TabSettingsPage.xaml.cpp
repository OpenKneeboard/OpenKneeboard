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
#include "DCSRadioLogTabUIData.g.cpp"
#include "TabUIDataTemplateSelector.g.cpp"
#include "TabSettingsPage.g.cpp"
// clang-format on

#include <OpenKneeboard/DCSRadioLogTab.h>
#include <OpenKneeboard/FilePageSource.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/TabTypes.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/TabsList.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/inttypes.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/weak_wrap.h>
#include <microsoft.ui.xaml.window.h>
#include <shobjidl.h>

#include <ranges>
#include <shims/utility>

#include "FilePicker.h"
#include "Globals.h"

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {

OpenKneeboardApp::TabUIData TabSettingsPage::CreateTabUIData(
  const std::shared_ptr<ITab>& tab) {
  OpenKneeboardApp::TabUIData tabData {nullptr};
  if (auto radioTab = std::dynamic_pointer_cast<DCSRadioLogTab>(tab)) {
    auto radioTabData = winrt::make<DCSRadioLogTabUIData>();
    tabData = std::move(radioTabData);
  } else {
    tabData = std::move(winrt::make<TabUIData>());
  }
  tabData.InstanceID(tab->GetRuntimeID().GetTemporaryValue());
  return tabData;
}

TabSettingsPage::TabSettingsPage() {
  InitializeComponent();

  auto tabTypes = AddTabFlyout().Items();
#define IT(label, name) \
  MenuFlyoutItem name##Item; \
  name##Item.Text(to_hstring(label)); \
  name##Item.Tag(box_value<uint64_t>(TABTYPE_IDX_##name)); \
  name##Item.Click({this, &TabSettingsPage::CreateTab}); \
  tabTypes.Append(name##Item);
  OPENKNEEBOARD_TAB_TYPES
#undef IT

  AddEventListener(
    gKneeboard->GetTabsList()->evTabsChangedEvent,
    weak_wrap(
      [this](auto) {
        if (mUIIsChangingTabs) {
          return;
        }
        if (mPropertyChangedEvent) {
          mPropertyChangedEvent(*this, PropertyChangedEventArgs(L"Tabs"));
        }
      },
      this));
}

IVector<IInspectable> TabSettingsPage::Tabs() noexcept {
  auto tabs = winrt::single_threaded_observable_vector<IInspectable>();
  for (const auto& tab: gKneeboard->GetTabsList()->GetTabs()) {
    tabs.Append(CreateTabUIData(tab));
  }
  tabs.VectorChanged({this, &TabSettingsPage::OnTabsChanged});
  return tabs;
}

TabSettingsPage::~TabSettingsPage() {
  this->RemoveAllEventListeners();
}

fire_and_forget TabSettingsPage::RestoreDefaults(
  const IInspectable&,
  const RoutedEventArgs&) noexcept {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_("Restore defaults?"))));
  dialog.Content(
    box_value(to_hstring(_("Do you want to restore the default tabs list, "
                           "removing your preferences?"))));
  dialog.PrimaryButtonText(to_hstring(_("Restore Defaults")));
  dialog.CloseButtonText(to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Close);

  if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  gKneeboard->ResetTabsSettings();
}

fire_and_forget TabSettingsPage::RenameTab(
  const IInspectable& sender,
  const RoutedEventArgs&) {
  const auto tabID = ITab::RuntimeID::FromTemporaryValue(
    unbox_value<uint64_t>(sender.as<Button>().Tag()));
  auto tabsList = gKneeboard->GetTabsList();
  const auto tabs = tabsList->GetTabs();
  auto it = std::ranges::find(tabs, tabID, &ITab::GetRuntimeID);
  if (it == tabs.end()) {
    co_return;
  }
  const auto& tab = *it;

  TextBlock promptText;
  promptText.Text(_(L"What would you like to rename this tab to?"));

  TextBox tabName;
  tabName.Text(to_hstring(tab->GetTitle()));
  tabName.PlaceholderText(to_hstring(tab->GetTitle()));
  tabName.Focus(FocusState::Programmatic);
  tabName.SelectAll();

  StackPanel layout;
  layout.Margin({8, 8, 8, 8});
  layout.Spacing(4);
  layout.Orientation(Orientation::Vertical);
  layout.Children().Append(promptText);
  layout.Children().Append(tabName);

  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(
    box_value(to_hstring(std::format(_("Rename '{}'"), tab->GetTitle()))));
  dialog.Content(layout);

  dialog.PrimaryButtonText(to_hstring(_("Rename")));
  dialog.CloseButtonText(to_hstring(_("Cancel")));
  dialog.DefaultButton(ContentDialogButton::Primary);

  auto result = co_await dialog.ShowAsync();
  if (result != ContentDialogResult::Primary) {
    co_return;
  }

  const auto newName = to_string(tabName.Text());
  if (newName.empty()) {
    co_return;
  }
  tab->SetTitle(newName);
}

fire_and_forget TabSettingsPage::RemoveTab(
  const IInspectable& sender,
  const RoutedEventArgs&) {
  const auto tabID = ITab::RuntimeID::FromTemporaryValue(
    unbox_value<uint64_t>(sender.as<Button>().Tag()));
  auto tabsList = gKneeboard->GetTabsList();
  const auto tabs = tabsList->GetTabs();
  auto it = std::ranges::find(tabs, tabID, &ITab::GetRuntimeID);
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

  mUIIsChangingTabs = true;
  const scope_guard changeComplete {[&]() { mUIIsChangingTabs = false; }};

  auto idx = static_cast<OpenKneeboard::TabIndex>(it - tabs.begin());
  tabsList->RemoveTab(idx);
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
      CreateFolderTab();
      return;
    case TabType::SingleFile:
      CreateFileTab();
      return;
  }
#define IT(_, type) \
  if (tabType == TabType::type) { \
    if constexpr (std::constructible_from< \
                    type##Tab, \
                    DXResources, \
                    KneeboardState*>) { \
      AddTabs({std::make_shared<type##Tab>(gDXResources, gKneeboard.get())}); \
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

void TabSettingsPage::CreateFileTab() {
  constexpr winrt::guid thisCall {
    0x207fb217,
    0x12fc,
    0x473c,
    {0xad, 0x36, 0x6d, 0x2c, 0xdb, 0xed, 0xa9, 0xc0}};

  FilePicker picker(gMainWindow);
  picker.SettingsIdentifier(thisCall);
  picker.SuggestedStartLocation(FOLDERID_Documents);
  std::vector<std::wstring> extensions;
  for (const auto& utf8: FilePageSource::GetSupportedExtensions(gDXResources)) {
    extensions.push_back(std::wstring {to_hstring(utf8)});
  }
  picker.AppendFileType(L"Supported files", extensions);
  for (const auto& extension: extensions) {
    picker.AppendFileType(std::format(L"{} files", extension), {extension});
  }

  auto files = picker.PickMultipleFiles();
  if (files.empty()) {
    return;
  }

  std::vector<std::shared_ptr<OpenKneeboard::ITab>> newTabs;
  for (const auto& path: files) {
    newTabs.push_back(
      std::make_shared<SingleFileTab>(gDXResources, gKneeboard.get(), path));
  }

  this->AddTabs(newTabs);
}

void TabSettingsPage::CreateFolderTab() {
  constexpr winrt::guid thisCall {
    0xae9b7e43,
    0x5109,
    0x4b16,
    {0x8d, 0xfa, 0xef, 0xf6, 0xe6, 0xaf, 0x06, 0x28}};
  FilePicker picker(gMainWindow);
  picker.SettingsIdentifier(thisCall);
  picker.SuggestedStartLocation(FOLDERID_Documents);

  auto folder = picker.PickSingleFolder();
  if (!folder) {
    return;
  }

  this->AddTabs(
    {std::make_shared<FolderTab>(gDXResources, gKneeboard.get(), *folder)});
}

void TabSettingsPage::AddTabs(const std::vector<std::shared_ptr<ITab>>& tabs) {
  mUIIsChangingTabs = true;
  const scope_guard changeComplete {[&]() { mUIIsChangingTabs = false; }};

  auto initialIndex = List().SelectedIndex();
  if (initialIndex == -1) {
    initialIndex = 0;
  }

  auto tabsList = gKneeboard->GetTabsList();

  auto idx = initialIndex;
  auto allTabs = tabsList->GetTabs();
  for (const auto& tab: tabs) {
    allTabs.insert(allTabs.begin() + idx++, tab);
  }
  tabsList->SetTabs(allTabs);

  idx = initialIndex;
  auto items = List()
                 .ItemsSource()
                 .as<Windows::Foundation::Collections::IVector<IInspectable>>();
  for (const auto& tab: tabs) {
    items.InsertAt(idx++, CreateTabUIData(tab));
  }
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
  auto tabsList = gKneeboard->GetTabsList();
  auto tabs = tabsList->GetTabs();
  if (items.Size() != tabs.size()) {
    // ignore the deletion ...
    return;
  }
  // ... but act on the insert :)
  mUIIsChangingTabs = true;
  const scope_guard changeComplete {[&]() { mUIIsChangingTabs = false; }};

  std::vector<std::shared_ptr<ITab>> reorderedTabs;
  for (auto item: items) {
    auto id
      = ITab::RuntimeID::FromTemporaryValue(item.as<TabUIData>()->InstanceID());
    auto it = std::ranges::find(tabs, id, &ITab::GetRuntimeID);
    if (it == tabs.end()) {
      continue;
    }
    reorderedTabs.push_back(*it);
  }
  tabsList->SetTabs(reorderedTabs);
}

TabUIData::~TabUIData() {
  this->RemoveAllEventListeners();
}

hstring TabUIData::Title() const {
  const auto tab = mTab.lock();
  if (!tab) {
    return {};
  }
  return to_hstring(tab->GetTitle());
}

uint64_t TabUIData::InstanceID() const {
  const auto tab = mTab.lock();
  if (!tab) {
    return {};
  }
  return tab->GetRuntimeID().GetTemporaryValue();
}

void TabUIData::InstanceID(uint64_t value) {
  this->RemoveAllEventListeners();
  mTab = {};

  const auto tabs = gKneeboard->GetTabsList()->GetTabs();
  const auto it = std::ranges::find_if(
    tabs, [value](const auto& tab) { return tab->GetRuntimeID() == value; });
  if (it == tabs.end()) {
    return;
  }

  const auto tab = *it;
  mTab = tab;

  AddEventListener(tab->evSettingsChangedEvent, [weakThis = get_weak()]() {
    auto strongThis = weakThis.get();
    if (!strongThis) {
      return;
    }
    strongThis->mPropertyChangedEvent(
      *strongThis, PropertyChangedEventArgs(L"Title"));
  });
}

std::shared_ptr<DCSRadioLogTab> DCSRadioLogTabUIData::GetTab() const {
  auto tab = mTab.lock();
  if (!tab) {
    return {};
  }
  auto refined = std::dynamic_pointer_cast<DCSRadioLogTab>(tab);
  if (!refined) {
    dprint("Expected a DCSRadioLogTab but didn't get one");
    OPENKNEEBOARD_BREAK;
  }
  return refined;
}

uint8_t DCSRadioLogTabUIData::MissionStartBehavior() const {
  auto tab = this->GetTab();
  return tab ? std23::to_underlying(tab->GetMissionStartBehavior()) : 0;
}

void DCSRadioLogTabUIData::MissionStartBehavior(uint8_t value) {
  auto tab = this->GetTab();
  if (!tab) {
    return;
  }
  tab->SetMissionStartBehavior(
    static_cast<DCSRadioLogTab::MissionStartBehavior>(value));
}

bool DCSRadioLogTabUIData::TimestampsEnabled() const {
  return this->GetTab()->GetTimestampsEnabled();
}

void DCSRadioLogTabUIData::TimestampsEnabled(bool value) {
  this->GetTab()->SetTimestampsEnabled(value);
}

winrt::Microsoft::UI::Xaml::DataTemplate TabUIDataTemplateSelector::Generic() {
  return mGeneric;
}

void TabUIDataTemplateSelector::Generic(
  winrt::Microsoft::UI::Xaml::DataTemplate const& value) {
  mGeneric = value;
}

winrt::Microsoft::UI::Xaml::DataTemplate
TabUIDataTemplateSelector::DCSRadioLog() {
  return mDCSRadioLog;
}

void TabUIDataTemplateSelector::DCSRadioLog(
  winrt::Microsoft::UI::Xaml::DataTemplate const& value) {
  mDCSRadioLog = value;
}

DataTemplate TabUIDataTemplateSelector::SelectTemplateCore(
  const IInspectable& item) {
  if (item.try_as<winrt::OpenKneeboardApp::DCSRadioLogTabUIData>()) {
    return mDCSRadioLog;
  }

  return mGeneric;
}

DataTemplate TabUIDataTemplateSelector::SelectTemplateCore(
  const IInspectable& item,
  const DependencyObject&) {
  return this->SelectTemplateCore(item);
}

winrt::event_token TabSettingsPage::PropertyChanged(
  winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const&
    handler) {
  return mPropertyChangedEvent.add(handler);
}

void TabSettingsPage::PropertyChanged(
  winrt::event_token const& token) noexcept {
  mPropertyChangedEvent.remove(token);
}

winrt::event_token TabUIData::PropertyChanged(
  winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const&
    handler) {
  return mPropertyChangedEvent.add(handler);
}

void TabUIData::PropertyChanged(winrt::event_token const& token) noexcept {
  mPropertyChangedEvent.remove(token);
}

}// namespace winrt::OpenKneeboardApp::implementation
