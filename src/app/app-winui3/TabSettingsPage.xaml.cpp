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
#include "TabSettingsPage.g.cpp"

#include "TabUIData.g.cpp"
#include "DCSRadioLogTabUIData.g.cpp"
#include "WindowCaptureTabUIData.g.cpp"

#include "TabUIDataTemplateSelector.g.cpp"
// clang-format on

#include "FilePicker.h"
#include "Globals.h"

#include <OpenKneeboard/DCSRadioLogTab.h>
#include <OpenKneeboard/FilePageSource.h>
#include <OpenKneeboard/IHasDebugInformation.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/TabTypes.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/TabsList.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/inttypes.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/weak_wrap.h>

#include <shims/utility>

#include <microsoft.ui.xaml.window.h>

#include <ranges>

#include <shobjidl.h>

using namespace OpenKneeboard;
using namespace winrt::Microsoft::UI::Xaml::Controls;

using WindowSpec = WindowCaptureTab::WindowSpecification;

namespace winrt::OpenKneeboardApp::implementation {

OpenKneeboardApp::TabUIData TabSettingsPage::CreateTabUIData(
  const std::shared_ptr<ITab>& tab) {
  OpenKneeboardApp::TabUIData tabData {nullptr};
  if (std::dynamic_pointer_cast<DCSRadioLogTab>(tab)) {
    tabData = winrt::make<DCSRadioLogTabUIData>();
  } else if (std::dynamic_pointer_cast<WindowCaptureTab>(tab)) {
    tabData = winrt::make<WindowCaptureTabUIData>();
  } else {
    tabData = winrt::make<TabUIData>();
  }
  tabData.InstanceID(tab->GetRuntimeID().GetTemporaryValue());
  return tabData;
}

TabSettingsPage::TabSettingsPage() {
  InitializeComponent();
  mDXR = gDXResources.lock();

  AddEventListener(
    gKneeboard->GetTabsList()->evTabsChangedEvent,
    weak_wrap(this)([](auto self) {
      if (self->mUIIsChangingTabs) {
        return;
      }
      if (self->mPropertyChangedEvent) {
        self->mPropertyChangedEvent(*self, PropertyChangedEventArgs(L"Tabs"));
      }
    }));

  CreateAddTabMenu(AddTabTopButton(), FlyoutPlacementMode::Bottom);
  CreateAddTabMenu(AddTabBottomButton(), FlyoutPlacementMode::Top);
}

IVector<IInspectable> TabSettingsPage::Tabs() noexcept {
  const std::shared_lock kbLock(*gKneeboard);

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

void TabSettingsPage::CreateAddTabMenu(
  const Button& button,
  FlyoutPlacementMode placement) {
  MenuFlyout flyout;
  auto tabTypes = flyout.Items();
#define IT(label, name) \
  { \
    MenuFlyoutItem item; \
    item.Text(to_hstring(label)); \
    item.Tag(box_value<uint64_t>(TABTYPE_IDX_##name)); \
    item.Click({this, &TabSettingsPage::CreateTab}); \
    auto glyph = name##Tab::GetStaticGlyph(); \
    if (!glyph.empty()) { \
      FontIcon fontIcon; \
      fontIcon.Glyph(winrt::to_hstring(glyph)); \
      item.Icon(fontIcon); \
    } \
    tabTypes.Append(item); \
  }
  OPENKNEEBOARD_TAB_TYPES
#undef IT
  flyout.Placement(placement);
  button.Flyout(flyout);
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
  const std::shared_lock lock(*gKneeboard);

  const auto tabID = ITab::RuntimeID::FromTemporaryValue(
    unbox_value<uint64_t>(sender.as<Button>().Tag()));
  auto tabsList = gKneeboard->GetTabsList();
  const auto tabs = tabsList->GetTabs();
  auto it = std::ranges::find(tabs, tabID, &ITab::GetRuntimeID);
  if (it == tabs.end()) {
    co_return;
  }
  const auto& tab = *it;

  OpenKneeboardApp::RenameTabDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.TabTitle(to_hstring(tab->GetTitle()));

  auto result = co_await dialog.ShowAsync();
  if (result != ContentDialogResult::Primary) {
    co_return;
  }

  const auto newName = to_string(dialog.TabTitle());
  if (newName.empty()) {
    co_return;
  }
  tab->SetTitle(newName);
}

fire_and_forget TabSettingsPage::RemoveTab(
  const IInspectable& sender,
  const RoutedEventArgs&) {
  const std::shared_lock lock(*gKneeboard);

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
      CreateFileTab<SingleFileTab>();
      return;
    case TabType::EndlessNotebook:
      CreateFileTab<EndlessNotebookTab>(_("Open Template"));
      return;
    case TabType::WindowCapture:
      CreateWindowCaptureTab();
      return;
  }
#define IT(_, type) \
  if (tabType == TabType::type) { \
    if constexpr (std::constructible_from< \
                    type##Tab, \
                    std::shared_ptr<DXResources>, \
                    KneeboardState*>) { \
      AddTabs({std::make_shared<type##Tab>(mDXR, gKneeboard.get())}); \
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

winrt::fire_and_forget TabSettingsPage::CreateWindowCaptureTab() {
  OpenKneeboardApp::WindowPickerDialog picker;
  picker.XamlRoot(this->XamlRoot());

  if (co_await picker.ShowAsync() != ContentDialogResult::Primary) {
    co_return;
  }

  const auto hwnd = reinterpret_cast<HWND>(picker.Hwnd());
  if (!hwnd) {
    co_return;
  }

  const auto windowSpec = WindowCaptureTab::GetWindowSpecification(hwnd);
  if (!windowSpec) {
    co_return;
  }

  WindowCaptureTab::MatchSpecification matchSpec {*windowSpec};

  // WPF apps do not use window classes correctly
  if (windowSpec->mWindowClass.starts_with("HwndWrapper[")) {
    matchSpec.mMatchWindowClass = false;
    matchSpec.mMatchTitle
      = WindowCaptureTab::MatchSpecification::TitleMatchKind::Exact;
  }

  this->AddTabs({WindowCaptureTab::Create(mDXR, gKneeboard.get(), matchSpec)});
}

template <class T>
void TabSettingsPage::CreateFileTab(const std::string& pickerDialogTitle) {
  constexpr winrt::guid thisCall {
    0x207fb217,
    0x12fc,
    0x473c,
    {0xad, 0x36, 0x6d, 0x2c, 0xdb, 0xed, 0xa9, 0xc0}};

  FilePicker picker(gMainWindow);
  picker.SettingsIdentifier(thisCall);
  picker.SuggestedStartLocation(FOLDERID_Documents);
  std::vector<std::wstring> extensions;
  for (const auto& utf8: FilePageSource::GetSupportedExtensions(mDXR)) {
    extensions.push_back(std::wstring {to_hstring(utf8)});
  }
  picker.AppendFileType(L"Supported files", extensions);
  for (const auto& extension: extensions) {
    picker.AppendFileType(std::format(L"{} files", extension), {extension});
  }

  if (!pickerDialogTitle.empty()) {
    std::wstring utf16(to_hstring(pickerDialogTitle));
    picker.SetTitle(utf16);
  }

  auto files = picker.PickMultipleFiles();
  if (files.empty()) {
    return;
  }

  std::vector<std::shared_ptr<OpenKneeboard::ITab>> newTabs;
  for (const auto& path: files) {
    // TODO (after v1.4): figure out MSVC compile errors if I move
    // `detail::make_shared()` in TabTypes.h out of the `detail`
    // sub-namespace
    newTabs.push_back(detail::make_shared<T>(mDXR, gKneeboard.get(), path));
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

  this->AddTabs({std::make_shared<FolderTab>(mDXR, gKneeboard.get(), *folder)});
}

void TabSettingsPage::AddTabs(const std::vector<std::shared_ptr<ITab>>& tabs) {
  const std::shared_lock lock(*gKneeboard);

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
  const std::shared_lock lock(*gKneeboard);
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

bool TabUIData::HasDebugInformation() const {
  const auto tab = mTab.lock();
  return tab && std::dynamic_pointer_cast<IHasDebugInformation>(tab);
}

hstring TabUIData::DebugInformation() const {
  const auto tab = mTab.lock();
  if (!tab) {
    return {};
  }
  const auto hdi = std::dynamic_pointer_cast<IHasDebugInformation>(tab);
  if (!hdi) {
    return {};
  }
  return winrt::to_hstring(hdi->GetDebugInformation());
}

uint64_t TabUIData::InstanceID() const {
  const auto tab = mTab.lock();
  if (!tab) {
    return {};
  }
  return tab->GetRuntimeID().GetTemporaryValue();
}

void TabUIData::InstanceID(uint64_t value) {
  const std::shared_lock lock(*gKneeboard);

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

  const auto hdi = std::dynamic_pointer_cast<IHasDebugInformation>(tab);
  if (!hdi) {
    return;
  }

  AddEventListener(
    hdi->evDebugInformationHasChanged, [weakThis = get_weak()]() {
      auto self = weakThis.get();
      if (!self) {
        return;
      }
      self->mPropertyChangedEvent(
        *self, PropertyChangedEventArgs(L"DebugInformation"));
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

winrt::hstring WindowCaptureTabUIData::WindowTitle() {
  return to_hstring(GetTab()->GetMatchSpecification().mTitle);
}

void WindowCaptureTabUIData::WindowTitle(const hstring& title) {
  auto spec = GetTab()->GetMatchSpecification();
  spec.mTitle = to_string(title);
  GetTab()->SetMatchSpecification(spec);
}

bool WindowCaptureTabUIData::MatchWindowClass() {
  return GetTab()->GetMatchSpecification().mMatchWindowClass;
}

void WindowCaptureTabUIData::MatchWindowClass(bool value) {
  auto spec = GetTab()->GetMatchSpecification();
  spec.mMatchWindowClass = value;
  GetTab()->SetMatchSpecification(spec);
}

uint8_t WindowCaptureTabUIData::MatchWindowTitle() {
  return static_cast<uint8_t>(GetTab()->GetMatchSpecification().mMatchTitle);
}

void WindowCaptureTabUIData::MatchWindowTitle(uint8_t value) {
  auto spec = GetTab()->GetMatchSpecification();
  spec.mMatchTitle
    = static_cast<WindowCaptureTab::MatchSpecification::TitleMatchKind>(value);
  GetTab()->SetMatchSpecification(spec);
}

bool WindowCaptureTabUIData::IsInputEnabled() const {
  return GetTab()->IsInputEnabled();
}

void WindowCaptureTabUIData::IsInputEnabled(bool value) {
  GetTab()->SetIsInputEnabled(value);
}

bool WindowCaptureTabUIData::IsCursorCaptureEnabled() const {
  return GetTab()->IsCursorCaptureEnabled();
}

void WindowCaptureTabUIData::IsCursorCaptureEnabled(bool value) {
  GetTab()->SetCursorCaptureEnabled(value);
}

bool WindowCaptureTabUIData::CaptureClientArea() const {
  return GetTab()->GetCaptureArea() == HWNDPageSource::CaptureArea::ClientArea;
}

void WindowCaptureTabUIData::CaptureClientArea(bool enabled) {
  GetTab()->SetCaptureArea(
    enabled ? HWNDPageSource::CaptureArea::ClientArea
            : HWNDPageSource::CaptureArea::FullWindow);
}

std::shared_ptr<WindowCaptureTab> WindowCaptureTabUIData::GetTab() const {
  auto tab = mTab.lock();
  if (!tab) {
    return {};
  }
  auto refined = std::dynamic_pointer_cast<WindowCaptureTab>(tab);
  if (!refined) {
    dprint("Expected a WindowCaptureTab but didn't get one");
    OPENKNEEBOARD_BREAK;
  }
  return refined;
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

winrt::Microsoft::UI::Xaml::DataTemplate
TabUIDataTemplateSelector::WindowCapture() {
  return mWindowCapture;
}

void TabUIDataTemplateSelector::WindowCapture(
  winrt::Microsoft::UI::Xaml::DataTemplate const& value) {
  mWindowCapture = value;
}

DataTemplate TabUIDataTemplateSelector::SelectTemplateCore(
  const IInspectable& item) {
  if (item.try_as<winrt::OpenKneeboardApp::DCSRadioLogTabUIData>()) {
    return mDCSRadioLog;
  }

  if (item.try_as<winrt::OpenKneeboardApp::WindowCaptureTabUIData>()) {
    return mWindowCapture;
  }

  return mGeneric;
}

DataTemplate TabUIDataTemplateSelector::SelectTemplateCore(
  const IInspectable& item,
  const DependencyObject&) {
  return this->SelectTemplateCore(item);
}

}// namespace winrt::OpenKneeboardApp::implementation
