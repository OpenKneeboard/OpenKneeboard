// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once
// clang-format off
#include "pch.h"
#include "InputBindingsControl.xaml.h"
#include "InputBindingsControl.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/UserInputButtonBinding.hpp>
#include <OpenKneeboard/UserInputButtonEvent.hpp>
#include <OpenKneeboard/UserInputDevice.hpp>

#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <dinput.h>

#include <microsoft.ui.xaml.window.h>

#include <shobjidl.h>

namespace winrt::OpenKneeboardApp::implementation {
InputBindingsControl::InputBindingsControl() noexcept {
  this->InitializeComponent();
  this->PopulateUI();
}

void InputBindingsControl::PopulateUI() {
  AppendUIRow(UserAction::PREVIOUS_TAB, _(L"Previous tab"));
  AppendUIRow(UserAction::NEXT_TAB, _(L"Next tab"));

  AppendUIRow(UserAction::PREVIOUS_PAGE, _(L"Previous page"));
  AppendUIRow(UserAction::NEXT_PAGE, _(L"Next page"));

  AppendUIRow(UserAction::RECENTER_VR, _(L"Recenter VR"));
  AppendUIRow(UserAction::SWAP_FIRST_TWO_VIEWS, _(L"Swap first two views"));

  AppendUIRow(UserAction::TOGGLE_VISIBILITY, _(L"Show/hide"));
  AppendUIRow(UserAction::TOGGLE_FORCE_ZOOM, _(L"Toggle forced VR zoom"));

  AppendUIRow(UserAction::TOGGLE_TINT, _(L"Toggle custom tint and brightness"));
  AppendUIRow(UserAction::INCREASE_BRIGHTNESS, _(L"Increase brightness"));
  AppendUIRow(UserAction::DECREASE_BRIGHTNESS, _(L"Decrease brightness"));

  if (gKneeboard.lock()->GetUISettings().mBookmarks.mEnabled) {
    AppendUIRow(UserAction::PREVIOUS_BOOKMARK, _(L"Previous bookmark"));
    AppendUIRow(UserAction::NEXT_BOOKMARK, _(L"Next bookmark"));
    AppendUIRow(UserAction::TOGGLE_BOOKMARK, _(L"Add/remove bookmark"));
  }

  ContentGrid().UpdateLayout();
}

void InputBindingsControl::AppendUIRow(
  UserAction action,
  winrt::hstring label) {
  auto grid = ContentGrid();
  grid.RowDefinitions().Append({});
  const auto row = static_cast<int32_t>(mRows.size());
  auto AddToGrid =
    [&](Microsoft::UI::Xaml::FrameworkElement element, int32_t column) {
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

InputBindingsControl::~InputBindingsControl() {}

hstring InputBindingsControl::DeviceID() { return mDeviceID; }

void InputBindingsControl::DeviceID(const hstring& value) {
  mDeviceID = value;
  auto id = to_string(value);

  mDevice.reset();
  for (const auto& device: gKneeboard.lock()->GetInputDevices()) {
    if (device->GetID() == id) {
      mDevice = device;
      this->UpdateUI();
      return;
    }
  }
}

OpenKneeboard::fire_and_forget InputBindingsControl::PromptForBinding(
  UserAction action) {
  ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(_("Bind Buttons"))));
  dialog.Content(
    box_value(to_hstring(_("Press then release buttons to bind input"))));
  dialog.CloseButtonText(to_hstring(_("Cancel")));

  std::unordered_set<uint64_t> pressedButtons;
  bool cancelled = true;
  auto stayingAlive = this->get_strong();
  auto weakThis = this->get_weak();

  const bool isMouse =
    mDevice->GetID() == to_string(to_hstring({GUID_SysMouse}));
  const bool isKeyboard =
    mDevice->GetID() == to_string(to_hstring({GUID_SysKeyboard}));

  EventHookToken hookToken;
  auto unhook = scope_exit(
    [this, hookToken] { mDevice->evButtonEvent.RemoveHook(hookToken); });
  mDevice->evButtonEvent.AddHook(
    [hookToken,
     weakThis,
     dialog,
     &pressedButtons,
     &cancelled,
     isMouse,
     isKeyboard](const UserInputButtonEvent& ev) {
      auto strongThis = weakThis.get();
      if (!strongThis) {
        return EventBase::HookResult::ALLOW_PROPAGATION;
      }

      // Ban these as:
      // - they interfere with clicking the 'Cancel' button in the binding
      // prompt
      // - binding them is likely to conflict with general Windows usage and the
      // game

      const auto isLeftMouseButton = isMouse && (ev.GetButtonID() == 0);
      const auto isEscapeKey = isKeyboard && (ev.GetButtonID() == DIK_ESCAPE);

      const auto isBannedButtonBinding = isLeftMouseButton || isEscapeKey;

      if (ev.IsPressed()) {
        if (!isBannedButtonBinding) {
          pressedButtons.insert(ev.GetButtonID());
        }
        const auto bindingDesc = isBannedButtonBinding
          ? "[button can not be bound]"
          : strongThis->mDevice->GetButtonComboDescription(pressedButtons);

        [](auto strongThis, auto dialog, const auto bindingDesc)
          -> OpenKneeboard::fire_and_forget {
          co_await strongThis->mUIThread;
          dialog.Content(box_value(to_hstring(
            std::format(
              _("Press then release buttons to bind input.\n\n{}"),
              bindingDesc))));
        }(strongThis, dialog, bindingDesc);
        return EventBase::HookResult::STOP_PROPAGATION;
      }

      if (isBannedButtonBinding) {
        return EventBase::HookResult::ALLOW_PROPAGATION;
      }
      cancelled = false;
      strongThis->mDevice->evButtonEvent.RemoveHook(hookToken);

      [](
        auto strongThis,
        auto dialog) noexcept -> OpenKneeboard::fire_and_forget {
        // Show the complete combo for a moment
        co_await winrt::resume_after(std::chrono::milliseconds(250));
        co_await strongThis->mUIThread;
        dialog.Hide();
      }(strongThis, dialog);
      return EventBase::HookResult::STOP_PROPAGATION;
    },
    hookToken);
  co_await dialog.ShowAsync();

  if (cancelled) {
    co_return;
  }

  auto bindings = mDevice->GetButtonBindings();
  auto sameAction = std::ranges::find_if(
    bindings, [=](const auto& it) { return it.GetAction() == action; });
  if (sameAction != bindings.end()) {
    bindings.erase(sameAction);
  }
  while (true) {
    auto conflictingButtons =
      std::ranges::find_if(bindings, [&](const auto& it) {
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
