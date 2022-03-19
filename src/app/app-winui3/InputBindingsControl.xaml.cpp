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
InputBindingsControl::InputBindingsControl() {
  this->InitializeComponent();

  ///// bind buttons /////

  ToggleVisibilityBindButton().Click([this](auto&, auto&) {
    this->PromptForBinding(UserAction::TOGGLE_VISIBILITY);
  });
  ToggleForceZoomBindButton().Click([this](auto&, auto&) {
    this->PromptForBinding(UserAction::TOGGLE_FORCE_ZOOM);
  });
  PreviousPageBindButton().Click([this](auto&, auto&) {
    this->PromptForBinding(UserAction::PREVIOUS_PAGE);
  });
  NextPageBindButton().Click(
    [this](auto&, auto&) { this->PromptForBinding(UserAction::NEXT_PAGE); });
  PreviousTabBindButton().Click(
    [this](auto&, auto&) { this->PromptForBinding(UserAction::PREVIOUS_TAB); });
  NextTabBindButton().Click(
    [this](auto&, auto&) { this->PromptForBinding(UserAction::NEXT_TAB); });

  ///// clear buttons /////

  ToggleVisibilityClearButton().Click([this](auto&, auto&) {
    this->ClearBinding(UserAction::TOGGLE_VISIBILITY);
  });
  ToggleForceZoomClearButton().Click([this](auto&, auto&) {
    this->ClearBinding(UserAction::TOGGLE_FORCE_ZOOM);
  });
  PreviousPageClearButton().Click(
    [this](auto&, auto&) { this->ClearBinding(UserAction::PREVIOUS_PAGE); });
  NextPageClearButton().Click(
    [this](auto&, auto&) { this->ClearBinding(UserAction::NEXT_PAGE); });
  PreviousTabClearButton().Click(
    [this](auto&, auto&) { this->ClearBinding(UserAction::PREVIOUS_TAB); });
  NextTabClearButton().Click(
    [this](auto&, auto&) { this->ClearBinding(UserAction::NEXT_TAB); });
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
  UpdateUI(
    UserAction::TOGGLE_VISIBILITY,
    ToggleVisibilityLabel(),
    ToggleVisibilityBindButton(),
    ToggleVisibilityClearButton());
  UpdateUI(
    UserAction::TOGGLE_FORCE_ZOOM,
    ToggleForceZoomLabel(),
    ToggleForceZoomBindButton(),
    ToggleForceZoomClearButton());
  UpdateUI(
    UserAction::PREVIOUS_PAGE,
    PreviousPageLabel(),
    PreviousPageBindButton(),
    PreviousPageClearButton());
  UpdateUI(
    UserAction::NEXT_PAGE,
    NextPageLabel(),
    NextPageBindButton(),
    NextPageClearButton());
  UpdateUI(
    UserAction::PREVIOUS_TAB,
    PreviousTabLabel(),
    PreviousTabBindButton(),
    PreviousTabClearButton());
  UpdateUI(
    UserAction::NEXT_TAB,
    NextTabLabel(),
    NextTabBindButton(),
    NextTabClearButton());
}

void InputBindingsControl::UpdateUI(
  UserAction action,
  TextBlock label,
  Button bindButton,
  Button clearButton) {
  auto bindings = mDevice->GetButtonBindings();
  auto it = std::ranges::find_if(
    bindings, [=](auto& it) { return it.GetAction() == action; });
  if (it == bindings.end()) {
    label.Text(to_hstring(_("not bound")));
    clearButton.IsEnabled(false);
    return;
  }

  clearButton.IsEnabled(true);
  const auto& binding = *it;
  label.Text(
    to_hstring(mDevice->GetButtonComboDescription(binding.GetButtonIDs())));
}

}// namespace winrt::OpenKneeboardApp::implementation
