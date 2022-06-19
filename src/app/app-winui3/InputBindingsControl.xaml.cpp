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
#include "InputBindingsControl.xaml.h"
#include "InputBindingsControl.g.cpp"
// clang-format on

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/UserInputButtonBinding.h>
#include <OpenKneeboard/UserInputButtonEvent.h>
#include <OpenKneeboard/UserInputDevice.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/utf8.h>
#include <microsoft.ui.xaml.window.h>
#include <shobjidl.h>

#include "Globals.h"

namespace winrt::OpenKneeboardApp::implementation {
InputBindingsControl::InputBindingsControl() noexcept {
  this->InitializeComponent();
  this->PopulateUI();
}

void InputBindingsControl::PopulateUI() {
  AppendUIRow(UserAction::TOGGLE_VISIBILITY, _(L"Show/hide"));
  AppendUIRow(UserAction::TOGGLE_FORCE_ZOOM, _(L"Toggle forced VR zoom"));
  AppendUIRow(UserAction::RECENTER_VR, _(L"Recenter kneeboard"));

  AppendUIRow(UserAction::PREVIOUS_TAB, _(L"Previous tab"));
  AppendUIRow(UserAction::NEXT_TAB, _(L"Next tab"));

  AppendUIRow(UserAction::PREVIOUS_PAGE, _(L"Previous page"));
  AppendUIRow(UserAction::NEXT_PAGE, _(L"Next page"));
}

void InputBindingsControl::AppendUIRow(
  UserAction action,
  winrt::hstring label) {
  auto grid = ContentGrid();
  const auto row = static_cast<int32_t>(mRows.size());
  auto AddToGrid
    = [&](Microsoft::UI::Xaml::FrameworkElement element, int32_t column) {
        grid.Children().Append(element);
        Grid::SetColumn(element, column);
        Grid::SetRow(element, row);
      };

  TextBlock labelText;
  labelText.Style(this->Resources()
                    .Lookup(box_value(L"BodyTextBlockStyle"))
                    .as<Microsoft::UI::Xaml::Style>());
  labelText.Text(label);
  AddToGrid(labelText, 0);

  TextBlock bindingText;
  bindingText.HorizontalTextAlignment(
    Microsoft::UI::Xaml::TextAlignment::Center);
  bindingText.Foreground(this->Resources()
                           .Lookup(box_value(L"TextFillColorSecondary"))
                           .as<Microsoft::UI::Xaml::Media::Brush>());
  AddToGrid(bindingText, 1);

  Button bindButton;
  bindButton.Content(box_value(_(L"Bind")));
  AddToGrid(bindButton, 2);

  Button clearButton;
  clearButton.Content(box_value(_(L"Clear Binding")));
  AddToGrid(clearButton, 3);

  bindButton.Click([=](auto&, auto&) { this->PromptForBinding(action); });
  clearButton.Click([=](auto&, auto&) { this->ClearBinding(action); });

  mRows.emplace(action, Row {bindingText, bindButton, clearButton});
}

InputBindingsControl::~InputBindingsControl() {
}

hstring InputBindingsControl::DeviceID() {
  return mDeviceID;
}

void InputBindingsControl::DeviceID(const hstring& value) {
  mDeviceID = value;
  auto id = to_string(value);

  mDevice.reset();
  for (const auto& device: gKneeboard->GetInputDevices()) {
    if (device->GetID() == id) {
      mDevice = device;
      this->UpdateUI();
      return;
    }
  }
}

fire_and_forget InputBindingsControl::PromptForBinding(UserAction action) {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_("Bind Buttons"))));
  dialog.Content(
    box_value(to_hstring(_("Press then release buttons to bind input"))));
  dialog.CloseButtonText(to_hstring(_("Cancel")));

  std::unordered_set<uint64_t> pressedButtons;
  bool cancelled = true;
  mDevice->evButtonEvent.PushHook([&](const UserInputButtonEvent& ev) {
    if (ev.IsPressed()) {
      pressedButtons.insert(ev.GetButtonID());
      return EventBase::HookResult::STOP_PROPAGATION;
    }
    cancelled = false;
    dialog.Hide();
    return EventBase::HookResult::STOP_PROPAGATION;
  });
  {
    auto unhook = scope_guard([=] { mDevice->evButtonEvent.PopHook(); });
    co_await dialog.ShowAsync();
  }
  if (cancelled) {
    return;
  }

  auto bindings = mDevice->GetButtonBindings();
  auto sameAction = std::ranges::find_if(
    bindings, [=](const auto& it) { return it.GetAction() == action; });
  if (sameAction != bindings.end()) {
    bindings.erase(sameAction);
  }
  while (true) {
    auto conflictingButtons
      = std::ranges::find_if(bindings, [&](const auto& it) {
          for (auto button: it.GetButtonIDs()) {
            if (!pressedButtons.contains(button)) {
              return false;
            }
          }
          return true;
        });
    if (conflictingButtons == bindings.end()) {
      break;
    }
    bindings.erase(conflictingButtons);
  }

  bindings.push_back({mDevice, pressedButtons, action});
  mDevice->SetButtonBindings(bindings);
  UpdateUI();
}

void InputBindingsControl::ClearBinding(UserAction action) {
  auto bindings = mDevice->GetButtonBindings();
  auto it = std::ranges::find_if(
    bindings, [=](const auto& it) { return it.GetAction() == action; });
  if (it == bindings.end()) {
    return;
  }

  bindings.erase(it);
  mDevice->SetButtonBindings(bindings);
  UpdateUI();
}

void InputBindingsControl::UpdateUI() {
  for (const auto& [action, row]: mRows) {
    UpdateUI(action);
  }
}

void InputBindingsControl::UpdateUI(UserAction action) {
  auto& row = mRows.at(action);

  auto bindings = mDevice->GetButtonBindings();
  auto it = std::ranges::find_if(
    bindings, [=](auto& it) { return it.GetAction() == action; });

  if (it == bindings.end()) {
    row.mCurrentBinding.Text(to_hstring(_("not bound")));
    row.mClearButton.IsEnabled(false);
    return;
  }

  row.mClearButton.IsEnabled(true);
  const auto& binding = *it;
  row.mCurrentBinding.Text(
    to_hstring(mDevice->GetButtonComboDescription(binding.GetButtonIDs())));
}

}// namespace winrt::OpenKneeboardApp::implementation
