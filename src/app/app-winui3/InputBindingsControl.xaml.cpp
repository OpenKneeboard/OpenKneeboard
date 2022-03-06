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
#include <OpenKneeboard/UserInputDevice.h>
#include <OpenKneeboard/utf8.h>

#include "Globals.h"

namespace winrt::OpenKneeboardApp::implementation {
InputBindingsControl::InputBindingsControl() {
  this->InitializeComponent();
  ToggleVisibilityClearButton().Click([this](auto&, auto&) {
    this->ClearBinding(UserAction::TOGGLE_VISIBILITY);
  });
  PreviousPageClearButton().Click([this](auto&, auto&) {
    this->ClearBinding(UserAction::PREVIOUS_PAGE);
  });
  NextPageClearButton().Click([this](auto&, auto&) {
    this->ClearBinding(UserAction::NEXT_PAGE);
  });
  PreviousTabClearButton().Click([this](auto&, auto&) {
    this->ClearBinding(UserAction::PREVIOUS_TAB);
  });
  NextTabClearButton().Click([this](auto&, auto&) {
    this->ClearBinding(UserAction::NEXT_TAB);
  });
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
